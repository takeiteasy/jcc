/*
 JCC: JIT C Compiler

 Copyright (C) 2025 George Watson

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.

 This file was based on c4 by Robert Swierczek (rswier/c4) and the following
 write-a-C-interpreter tutorial by Jinzhou Zhang (lotabout/write-a-C-interpreter)
*/

#include "jcc.h"
#include "./internal.h"

// Heap canary constant for detecting heap overflows
#define HEAP_CANARY 0xCAFEBABEDEADBEEFLL

// Number of stack slots occupied by stack canary
#define STACK_CANARY_SLOTS 1

// Generate a random canary value for stack protection
long long generate_random_canary(void) {
    long long canary = 0;

#if defined(_WIN32) || defined(_WIN64)
    // Windows: Use rand() with time seed
    srand((unsigned int)time(NULL));
    canary = ((long long)rand() << 32) | rand();
#else
    // Unix-like: Try to read from /dev/urandom for cryptographic randomness
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(&canary, sizeof(canary), 1, f) != 1) {
            // Failed to read, fall back to time-based random
            fclose(f);
            srand((unsigned int)time(NULL));
            canary = ((long long)rand() << 32) | rand();
        } else {
            fclose(f);
        }
    } else {
        // /dev/urandom not available, fall back to time-based random
        srand((unsigned int)time(NULL));
        canary = ((long long)rand() << 32) | rand();
    }
#endif

    // Ensure canary is non-zero (zero would be too easy to produce accidentally)
    if (canary == 0)
        canary = STACK_CANARY;

    return canary;
}

// Segregated free list constants
#define NUM_SIZE_CLASSES 12
#define MAX_SMALL_ALLOC 8192

// Size classes for segregated free lists:
// 0:8, 1:16, 2:32, 3:64, 4:128, 5:256, 6:512, 7:1024, 8:2048, 9:4096, 10:8192, 11:LARGE

// Helper function: map size to size class index
// Returns index into size_class_lists array, or -1 for large allocations
static int size_to_class(size_t size) {
    // Binary search for appropriate size class
    if (size <= 8) return 0;
    if (size <= 16) return 1;
    if (size <= 32) return 2;
    if (size <= 64) return 3;
    if (size <= 128) return 4;
    if (size <= 256) return 5;
    if (size <= 512) return 6;
    if (size <= 1024) return 7;
    if (size <= 2048) return 8;
    if (size <= 4096) return 9;
    if (size <= 8192) return 10;
    return 11;  // LARGE class
}

// Forward declarations for sorted allocation array helpers
static int find_containing_allocation(JCC *vm, void *ptr);
static void insert_sorted_allocation(JCC *vm, void *address, AllocHeader *header);
static void remove_sorted_allocation(JCC *vm, void *address);

// Helper function to count format specifiers in a format string
// Returns -1 if format string is invalid or inaccessible
static int count_format_specifiers(const char *fmt) {
    if (!fmt) return -1;

    int count = 0;
    const char *p = fmt;

    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == '%') {
                // Literal %, not a format specifier
                p++;
                continue;
            }

            // Skip flags: -, +, space, #, 0
            while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') {
                p++;
            }

            // Skip width (digits or *)
            if (*p == '*') {
                count++;  // * consumes an argument
                p++;
            } else {
                while (*p >= '0' && *p <= '9') {
                    p++;
                }
            }

            // Skip precision (.digits or .*)
            if (*p == '.') {
                p++;
                if (*p == '*') {
                    count++;  // * consumes an argument
                    p++;
                } else {
                    while (*p >= '0' && *p <= '9') {
                        p++;
                    }
                }
            }

            // Skip length modifiers: hh, h, l, ll, L, z, j, t
            if (*p == 'h') {
                p++;
                if (*p == 'h') p++;  // hh
            } else if (*p == 'l') {
                p++;
                if (*p == 'l') p++;  // ll
            } else if (*p == 'L' || *p == 'z' || *p == 'j' || *p == 't') {
                p++;
            }

            // Check for valid conversion specifier
            // d, i, u, o, x, X, f, F, e, E, g, G, a, A, c, s, p, n
            if (*p == 'd' || *p == 'i' || *p == 'u' || *p == 'o' ||
                *p == 'x' || *p == 'X' || *p == 'f' || *p == 'F' ||
                *p == 'e' || *p == 'E' || *p == 'g' || *p == 'G' ||
                *p == 'a' || *p == 'A' || *p == 'c' || *p == 's' ||
                *p == 'p' || *p == 'n') {
                count++;
                p++;
            } else if (*p == '\0') {
                // Format string ends with incomplete specifier
                return -1;
            } else {
                // Unknown conversion specifier, skip it
                p++;
            }
        } else {
            p++;
        }
    }

    return count;
}

static void report_memory_leaks(JCC *vm) {
    if (!(vm->flags & JCC_MEMORY_LEAK_DETECT))
        return;

    if (!vm->alloc_list) {
        if (vm->debug_vm) {
            printf("\n========== NO MEMORY LEAKS DETECTED ==========\n");
            printf("All allocations were properly freed.\n");
            printf("============================================\n");
        }
        return;
    }

    int leak_count = 0;
    size_t total_leaked = 0;
    AllocRecord *curr = vm->alloc_list;
    while (curr) {
        leak_count++;
        total_leaked += curr->size;
        curr = curr->next;
    }

    printf("\n========== MEMORY LEAKS DETECTED ==========\n");
    printf("Found %d leaked allocation(s), totaling %zu bytes\n\n", leak_count, total_leaked);

    curr = vm->alloc_list;
    int i = 1;
    while (curr) {
        printf("Leak #%d:\n", i++);
        printf("  Address:  0x%llx\n", (long long)curr->address);
        printf("  Size:     %zu bytes\n", curr->size);
        printf("  Allocated at PC offset: %lld\n", curr->alloc_pc);
        printf("\n");

        AllocRecord *next = curr->next;
        free(curr);  // Clean up the tracking record
        curr = next;
    }

    printf("============================================\n");
    vm->alloc_list = NULL;
}

// ========== FREE LIST VALIDATION ==========

// Validate a free block to detect corruption
// Returns 1 if valid, 0 if corrupted
static int validate_free_block(JCC *vm, FreeBlock *block, const char *context) {
    if (!block) {
        return 1;  // NULL is valid (end of list)
    }

    // Check 1: Size must be non-zero (free blocks always have size)
    if (block->size == 0) {
        printf("\n========== FREE LIST CORRUPTION ==========\n");
        printf("Context: %s\n", context);
        printf("Block address: 0x%llx\n", (long long)block);
        printf("ERROR: Free block has zero size\n");
        printf("This indicates free list corruption.\n");
        printf("=========================================\n");
        return 0;
    }

    // Check 2: Size must not exceed total heap capacity
    if (block->size > (size_t)vm->poolsize) {
        printf("\n========== FREE LIST CORRUPTION ==========\n");
        printf("Context: %s\n", context);
        printf("Block address: 0x%llx\n", (long long)block);
        printf("Block size:    %zu bytes\n", block->size);
        printf("Heap capacity: %d bytes\n", vm->poolsize);
        printf("ERROR: Free block size exceeds heap capacity\n");
        printf("This indicates free list corruption.\n");
        printf("=========================================\n");
        return 0;
    }

    // Check 3: Block must be within heap bounds
    char *block_start = (char *)block;
    char *block_end = block_start + sizeof(AllocHeader) + block->size;
    if (block_start < vm->heap_seg || block_end > vm->heap_end) {
        printf("\n========== FREE LIST CORRUPTION ==========\n");
        printf("Context: %s\n", context);
        printf("Block address: 0x%llx\n", (long long)block);
        printf("Block size:    %zu bytes\n", block->size);
        printf("Block range:   [0x%llx - 0x%llx]\n",
               (long long)block_start, (long long)block_end);
        printf("Heap range:    [0x%llx - 0x%llx]\n",
               (long long)vm->heap_seg, (long long)vm->heap_end);
        printf("ERROR: Free block extends outside heap bounds\n");
        printf("This indicates free list corruption.\n");
        printf("=========================================\n");
        return 0;
    }

    return 1;  // Valid
}

// ========== FREE BLOCK COALESCING ==========

// Coalesce adjacent free blocks to reduce fragmentation
// This function merges contiguous free blocks in the free list

// Comparator for qsort: sort FreeBlocks by address
static int compare_blocks_by_address(const void *a, const void *b) {
    FreeBlock *block_a = *(FreeBlock **)a;
    FreeBlock *block_b = *(FreeBlock **)b;
    if (block_a < block_b) return -1;
    if (block_a > block_b) return 1;
    return 0;
}

// Coalesce adjacent free blocks across all segregated lists
// This is more complex than single-list coalescing because blocks can be in different size classes
static void coalesce_free_blocks(JCC *vm) {
    size_t canary_overhead = vm->flags & JCC_HEAP_CANARIES ? sizeof(long long) : 0;

    // Collect all free blocks into a temporary array for easier processing
    FreeBlock *all_blocks[1024];  // Reasonable limit
    int block_count = 0;

    // Gather blocks from all size classes
    for (int i = 0; i < NUM_SIZE_CLASSES - 1; i++) {
        FreeBlock *curr = vm->size_class_lists[i];
        while (curr && block_count < 1024) {
            all_blocks[block_count++] = curr;
            curr = curr->next;
        }
    }

    // Gather blocks from large list
    FreeBlock *curr = vm->large_list;
    while (curr && block_count < 1024) {
        all_blocks[block_count++] = curr;
        curr = curr->next;
    }

    if (block_count == 0) {
        return;  // No blocks to coalesce
    }

    // First, remove all blocks from their current size classes
    // This ensures we start with empty lists and rebuild them
    for (int i = 0; i < NUM_SIZE_CLASSES - 1; i++) {
        vm->size_class_lists[i] = NULL;
    }
    vm->large_list = NULL;

    // Sort blocks by address - O(n log n)
    qsort(all_blocks, block_count, sizeof(FreeBlock *), compare_blocks_by_address);

    // Single-pass merge of adjacent blocks - O(n)
    FreeBlock *merged_blocks[1024];
    int merged_count = 0;

    FreeBlock *current = all_blocks[0];
    char *current_start = (char *)current;
    char *current_end = current_start + sizeof(AllocHeader) + current->size + canary_overhead;

    for (int i = 1; i < block_count; i++) {
        FreeBlock *next = all_blocks[i];
        char *next_start = (char *)next;

        // Check if current and next are adjacent
        if (current_end == next_start) {
            // Adjacent! Merge next into current
            // Expand current block to include next block
            current->size += sizeof(AllocHeader) + next->size + canary_overhead;
            current_end = current_start + sizeof(AllocHeader) + current->size + canary_overhead;

            if (vm->debug_vm) {
                printf("COALESCE: merged adjacent blocks at 0x%llx (new size: %zu bytes)\n",
                       (long long)current, current->size);
            }
        } else {
            // Not adjacent - save current block and move to next
            merged_blocks[merged_count++] = current;
            current = next;
            current_start = next_start;
            current_end = current_start + sizeof(AllocHeader) + current->size + canary_overhead;
        }
    }

    // Don't forget the last block
    merged_blocks[merged_count++] = current;

    // Re-insert all merged blocks into appropriate size classes
    for (int i = 0; i < merged_count; i++) {
        FreeBlock *block = merged_blocks[i];
        int new_class = size_to_class(block->size);

        if (new_class < NUM_SIZE_CLASSES - 1) {
            block->next = vm->size_class_lists[new_class];
            vm->size_class_lists[new_class] = block;
        } else {
            block->next = vm->large_list;
            vm->large_list = block;
        }
    }
}

// ========== SORTED ALLOCATION ARRAY HELPERS ==========
// For O(log n) pointer validation in CHKP/CHKT opcodes

// Comparator for qsort/bsearch: compare base addresses
// Binary search to find allocation containing the given pointer
// Returns the index in sorted_allocs if found, -1 otherwise
static int find_containing_allocation(JCC *vm, void *ptr) {
    if (vm->sorted_allocs.count == 0) {
        return -1;
    }

    long long ptr_addr = (long long)ptr;

    // Binary search for the allocation that contains ptr
    // We need to find an allocation where: base <= ptr <= base + size
    int left = 0;
    int right = vm->sorted_allocs.count - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        void *base = vm->sorted_allocs.addresses[mid];
        AllocHeader *header = vm->sorted_allocs.headers[mid];
        long long base_addr = (long long)base;
        long long end_addr = base_addr + header->size;

        // Check if ptr is within this allocation's range
        if (ptr_addr >= base_addr && ptr_addr <= end_addr) {
            return mid;  // Found it!
        }

        // Adjust search range
        if (ptr_addr < base_addr) {
            right = mid - 1;
        } else {
            // ptr is after this allocation, but it might be in a later one
            left = mid + 1;
        }
    }

    return -1;  // Not found in any allocation
}

// Insert allocation into sorted array (maintains sorted order)
static void insert_sorted_allocation(JCC *vm, void *address, AllocHeader *header) {
    // Grow array if needed
    if (vm->sorted_allocs.count >= vm->sorted_allocs.capacity) {
        int new_capacity = vm->sorted_allocs.capacity == 0 ? 256 : vm->sorted_allocs.capacity * 2;

        void **new_addresses = (void **)realloc(vm->sorted_allocs.addresses,
                                                 new_capacity * sizeof(void *));
        AllocHeader **new_headers = (AllocHeader **)realloc(vm->sorted_allocs.headers,
                                                             new_capacity * sizeof(AllocHeader *));

        if (!new_addresses || !new_headers) {
            printf("FATAL: Failed to grow sorted_allocs array\n");
            exit(1);
        }

        vm->sorted_allocs.addresses = new_addresses;
        vm->sorted_allocs.headers = new_headers;
        vm->sorted_allocs.capacity = new_capacity;
    }

    // Binary search to find insertion point
    int left = 0;
    int right = vm->sorted_allocs.count;

    while (left < right) {
        int mid = left + (right - left) / 2;
        if (vm->sorted_allocs.addresses[mid] < address) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    // Shift elements to make room
    int insert_pos = left;
    for (int i = vm->sorted_allocs.count; i > insert_pos; i--) {
        vm->sorted_allocs.addresses[i] = vm->sorted_allocs.addresses[i - 1];
        vm->sorted_allocs.headers[i] = vm->sorted_allocs.headers[i - 1];
    }

    // Insert new allocation
    vm->sorted_allocs.addresses[insert_pos] = address;
    vm->sorted_allocs.headers[insert_pos] = header;
    vm->sorted_allocs.count++;
}

// Remove allocation from sorted array
static void remove_sorted_allocation(JCC *vm, void *address) {
    // Binary search to find the allocation
    int left = 0;
    int right = vm->sorted_allocs.count - 1;
    int found_index = -1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (vm->sorted_allocs.addresses[mid] == address) {
            found_index = mid;
            break;
        } else if (vm->sorted_allocs.addresses[mid] < address) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (found_index == -1) {
        // Not found - this can happen for quarantined blocks that were never added
        return;
    }

    // Shift elements to fill the gap
    for (int i = found_index; i < vm->sorted_allocs.count - 1; i++) {
        vm->sorted_allocs.addresses[i] = vm->sorted_allocs.addresses[i + 1];
        vm->sorted_allocs.headers[i] = vm->sorted_allocs.headers[i + 1];
    }

    vm->sorted_allocs.count--;
}

int op_LEA_fn(JCC *vm) {
    // Load effective address (bp + offset)
    long long offset = *vm->pc++;

    // If stack canaries are enabled and this is a local variable (negative offset),
    // we need to adjust by -STACK_CANARY_SLOTS because the canary occupies bp-1
    if (vm->flags & JCC_STACK_CANARIES && offset < 0) {
        offset -= STACK_CANARY_SLOTS;  // Shift locals down by canary slots
    }

    vm->ax = (long long)(vm->bp + offset);
    return 0;
}

int op_IMM_fn(JCC *vm) {
    vm->ax = *vm->pc++;
    return 0;
}

int op_JMP_fn(JCC *vm) {
    vm->pc = (long long *)*vm->pc;
    return 0;
}

int op_JMPI_fn(JCC *vm) {
    // Jump indirect - jump to address in ax
    vm->pc = (long long *)vm->ax;
    return 0;
}

int op_CALL_fn(JCC *vm) {
    // Call subroutine: push return address to main stack and shadow stack
    long long ret_addr = (long long)(vm->pc+1);
    *--vm->sp = ret_addr;
    if (vm->flags & JCC_CFI) {
        *--vm->shadow_sp = ret_addr;  // Also push to shadow stack for CFI
    }
    vm->pc = (long long *)*vm->pc;
    return 0;
}

int op_CALLI_fn(JCC *vm) {
    // Call subroutine indirect: push return address to main stack and shadow stack
    // Function address is in ax register
    long long ret_addr = (long long)vm->pc;
    *--vm->sp = ret_addr;
    if (vm->flags & JCC_CFI) {
        *--vm->shadow_sp = ret_addr;  // Also push to shadow stack for CFI
    }
    vm->pc = (long long *)vm->ax;  // Jump to address in ax
    return 0;
}

int op_JZ_fn(JCC *vm) {
    vm->pc = vm->ax ? vm->pc + 1 : (long long *)*vm->pc;
    return 0;
}

int op_JNZ_fn(JCC *vm) {
    vm->pc = vm->ax ? (long long *)*vm->pc : vm->pc + 1;
    return 0;
}

int op_JMPT_fn(JCC *vm) {
    // Jump table: ax contains index, *pc contains jump table base address
    long long *jump_table = (long long *)*vm->pc;
    long long target = jump_table[vm->ax];
    vm->pc = (long long *)target;
    return 0;
}

int op_ENT_fn(JCC *vm) {
    // Enter function: create new stack frame
    *--vm->sp = (long long)vm->bp;  // Save old base pointer
    vm->bp = vm->sp;                 // Set new base pointer

    // If stack canaries are enabled, write canary after old bp
    if (vm->flags & JCC_STACK_CANARIES) {
        *--vm->sp = vm->stack_canary;
    }

    // Allocate space for local variables
    long long stack_size = *vm->pc++;
    vm->sp = vm->sp - stack_size;

    // Stack overflow checking (for stack instrumentation)
    if (vm->flags & JCC_STACK_INSTR) {
        long long stack_used = (long long)(vm->initial_sp - vm->sp);
        if (stack_used > vm->stack_high_water) {
            vm->stack_high_water = stack_used;
        }

        // Check if we're approaching stack limit (within 10% of segment size)
        long long stack_limit = vm->poolsize * 9 / 10;  // 90% threshold
        if (stack_used > stack_limit) {
            if (vm->flags & JCC_STACK_INSTR_ERRORS) {
                printf("\n========== STACK OVERFLOW WARNING ==========\n");
                printf("Stack usage: %lld bytes (limit: %lld bytes)\n", stack_used, stack_limit);
                printf("Current PC: 0x%llx (offset: %lld)\n",
                        (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                printf("===========================================\n");
                return -1;
            } else if (vm->debug_vm) {
                printf("WARNING: Stack usage %lld bytes exceeds threshold\n", stack_used);
            }
        }
    }

    // Note: For uninitialized detection, local variables are uninitialized by default
    // (not in init_state HashMap). Parameters are marked as initialized by codegen.
    // Different function frames have different bp values, so init_state keys naturally
    // separate between frames.
    return 0;
}

int op_ADJ_fn(JCC *vm) {
    vm->sp = vm->sp + *vm->pc++;
    return 0;
}

int op_LEV_fn(JCC *vm) {
    // Leave function: restore stack frame and return
    vm->sp = vm->bp;  // Reset stack pointer to base pointer

    // If stack canaries are enabled, check canary (it's at bp-1)
    if (vm->flags & JCC_STACK_CANARIES) {
        // Canary is one slot below the saved bp
        long long canary = vm->sp[-1];
        if (canary != vm->stack_canary) {
            printf("\n========== STACK OVERFLOW DETECTED ==========\n");
            printf("Stack canary corrupted!\n");
            printf("Expected: 0x%llx\n", vm->stack_canary);
            printf("Found:    0x%llx\n", canary);
            printf("PC:       0x%llx (offset: %lld)\n",
                    (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
            printf("A stack buffer overflow has corrupted the stack frame.\n");
            printf("============================================\n");
            return -1;  // Abort execution
        }
    }

    // Invalidate stack pointers for this frame (dangling pointer detection)
    if (vm->flags & JCC_DANGLING_DETECT && vm->stack_ptrs.buckets) {
        // Iterate through all stack pointers and invalidate those with matching BP
        long long current_bp = (long long)vm->bp;
        for (int i = 0; i < vm->stack_ptrs.capacity; i++) {
            HashEntry *entry = &vm->stack_ptrs.buckets[i];
            if (entry->key && entry->key != (void *)-1) {  // Not NULL and not TOMBSTONE
                StackPtrInfo *info = (StackPtrInfo *)entry->val;
                if (info && info->bp == current_bp) {
                    // Mark as invalid by setting BP to -1
                    info->bp = -1;
                }
            }
        }
    }

    vm->bp = (long long *)*vm->sp++;  // Restore old base pointer

    // Get return address from main stack
    vm->pc = (long long *)*vm->sp++;  // Restore program counter (return address)

    // Check if we've returned from main (pc is NULL sentinel)
    // Skip CFI check for main's return since it was never called
    if (vm->pc == 0 || vm->pc == NULL) {
        // Main has returned, exit with return value in ax
        int ret_val = (int)vm->ax;

        // Report memory leaks if leak detection enabled
        report_memory_leaks(vm);

        return ret_val;
    }

    // CFI check: validate return address against shadow stack
    // Only for normal function returns (not main's exit)
    if (vm->flags & JCC_CFI) {
        long long shadow_ret_addr = *vm->shadow_sp++;  // Pop return address from shadow stack
        long long main_ret_addr = (long long)vm->pc;   // Return address we just loaded

        if (main_ret_addr != shadow_ret_addr) {
            printf("\n========== CFI VIOLATION ==========\n");
            printf("Control flow integrity violation detected!\n");
            printf("Expected return address: 0x%llx\n", shadow_ret_addr);
            printf("Actual return address:   0x%llx\n", main_ret_addr);
            printf("Current PC offset:       %lld\n",
                    (long long)(vm->pc - vm->text_seg));
            printf("This indicates a ROP attack or stack corruption.\n");
            printf("====================================\n");
            return -1;  // Abort execution
        }
    }
    return 0;
}

int op_LI_fn(JCC *vm) {
    // check read watchpoints before loading
    if (vm->flags & JCC_ENABLE_DEBUGGER && vm->dbg.num_watchpoints > 0) {
        debugger_check_watchpoint(vm, (void*)vm->ax, 8, WATCH_READ);
    }
    vm->ax = *(long long *)(long long)vm->ax; // load long long to ax, address in ax
    return 0;
}

int op_PUSH_fn(JCC *vm) {
    *--vm->sp = vm->ax; // push the value of ax onto the stack
    return 0;
}

int op_OR_fn(JCC *vm) {
    vm->ax = *vm->sp++ | vm->ax; // bitwise OR of ax and ax
    return 0;
}

int op_XOR_fn(JCC *vm) {
    vm->ax = *vm->sp++ ^ vm->ax; // bitwise XOR of ax and ax
    return 0;
}

int op_AND_fn(JCC *vm) {
    vm->ax = *vm->sp++ & vm->ax; // bitwise AND of ax and ax
    return 0;
}

int op_EQ_fn(JCC *vm) {
    vm->ax = *vm->sp++ == vm->ax; // equality comparison
    return 0;
}

int op_NE_fn(JCC *vm) {
    vm->ax = *vm->sp++ != vm->ax; // inequality comparison
    return 0;
}

int op_LT_fn(JCC *vm) {
    vm->ax = *vm->sp++ < vm->ax; // less than comparison
    return 0;
}

int op_GT_fn(JCC *vm) {
    vm->ax = *vm->sp++ > vm->ax; // greater than comparison
    return 0;
}

int op_LE_fn(JCC *vm) {
    vm->ax = *vm->sp++ <= vm->ax; // less than or equal to comparison
    return 0;
}

int op_GE_fn(JCC *vm) {
    vm->ax = *vm->sp++ >= vm->ax; // greater than or equal to comparison
    return 0;
}

int op_SHL_fn(JCC *vm) {
    vm->ax = *vm->sp++ << vm->ax; // left shift
    return 0;
}

int op_SHR_fn(JCC *vm) {
    vm->ax = *vm->sp++ >> vm->ax; // right shift
    return 0;
}

int op_ADD_fn(JCC *vm) {
    vm->ax = *vm->sp++ + vm->ax; // addition
    return 0;
}

int op_SUB_fn(JCC *vm) {
    vm->ax = *vm->sp++ - vm->ax; // subtraction
    return 0;
}

int op_MUL_fn(JCC *vm) {
    vm->ax = *vm->sp++ * vm->ax; // multiplication
    return 0;
}

int op_DIV_fn(JCC *vm) {
    vm->ax = *vm->sp++ / vm->ax; // division
    return 0;
}

int op_MOD_fn(JCC *vm) {
    vm->ax = *vm->sp++ % vm->ax; // modulo
    return 0;
}

int op_MALC_fn(JCC *vm) {
    // malloc: pop size from stack, allocate from heap, return pointer in ax
    long long requested_size = *vm->sp++;
    if (requested_size <= 0) {
        vm->ax = 0;  // Return NULL for invalid size
        return 0;
    }

    // Align to 8-byte boundary
    size_t size = (requested_size + 7) & ~7;

    // If heap canaries enabled, add space for rear canary (front canary is in AllocHeader)
    size_t canary_overhead = vm->flags & JCC_HEAP_CANARIES ? sizeof(long long) : 0;
    size_t total_size = size + sizeof(AllocHeader) + canary_overhead;

    // Try to find a suitable block using segregated free lists
    // This provides O(1) allocation for common sizes
    int size_class = size_to_class(size);
    FreeBlock *block = NULL;
    FreeBlock **list_ptr = NULL;

    // Try the appropriate size class first
    if (size_class < 11) {  // Not LARGE class
        list_ptr = &vm->size_class_lists[size_class];
        if (*list_ptr) {
            // Found a block in the exact size class
            block = *list_ptr;
            if (validate_free_block(vm, block, "MALC segregated list")) {
                *list_ptr = block->next;  // Remove from list
            } else {
                block = NULL;  // Corruption detected
            }
        }
    }

    // If no exact match, try next larger size classes
    if (!block && size_class < 11) {
        for (int i = size_class + 1; i < 11; i++) {
            if (vm->size_class_lists[i]) {
                list_ptr = &vm->size_class_lists[i];
                block = *list_ptr;
                if (validate_free_block(vm, block, "MALC segregated list")) {
                    *list_ptr = block->next;  // Remove from list
                    break;
                } else {
                    block = NULL;  // Corruption detected
                }
            }
        }
    }

    // If still no block, try large list with best-fit
    if (!block && vm->large_list) {
        FreeBlock **prev = &vm->large_list;
        FreeBlock *curr = vm->large_list;
        FreeBlock **best_prev = NULL;
        size_t best_size = SIZE_MAX;

        while (curr) {
            if (!validate_free_block(vm, curr, "MALC large list")) {
                break;
            }
            if (curr->size >= size && curr->size < best_size) {
                block = curr;
                best_prev = prev;
                best_size = curr->size;
                if (curr->size == size) break;  // Perfect fit
            }
            prev = &curr->next;
            curr = curr->next;
        }

        if (block && best_prev) {
            *best_prev = block->next;  // Remove from large list
        }
    }

    // If we found a suitable block, use it
    if (block) {
        // Remove block from whichever list it came from (already done above)

        // Restore the header (it was overwritten by FreeBlock)
        // NOTE: FreeBlock only overwrites first 16 bytes (next+size), so generation is preserved!
        AllocHeader *header = (AllocHeader *)block;
        int old_generation = header->generation;  // Preserve generation from previous allocation
        header->size = block->size;  // Use the actual block size
        header->requested_size = size;  // Store requested size for bounds checking
        header->magic = 0xDEADBEEF;
        header->freed = 0;
        header->generation = old_generation;  // Keep generation (will be incremented on next free)
        header->alloc_pc = vm->text_seg ? (long long)(vm->pc - vm->text_seg) : 0;
        header->type_kind = TY_VOID;  // Default to void* (generic pointer)

        // If heap canaries enabled, write canaries
        if (vm->flags & JCC_HEAP_CANARIES) {
            header->canary = HEAP_CANARY;
            // Write rear canary after user data
            long long *rear_canary = (long long *)((char *)(header + 1) + header->size);
            *rear_canary = HEAP_CANARY;
        }

        vm->ax = (long long)(header + 1);  // Return pointer after header

        // Add to sorted array for O(log n) pointer validation
        insert_sorted_allocation(vm, (void *)vm->ax, header);

        // If memory poisoning enabled, fill with 0xCD pattern
        if (vm->flags & JCC_MEMORY_POISONING) {
            memset((void *)vm->ax, 0xCD, header->size);
        }

        // If memory tagging enabled, record creation generation in header
        if (vm->flags & JCC_MEMORY_TAGGING) {
            header->creation_generation = header->generation;

            if (vm->debug_vm) {
                printf("MALC: tagged reused pointer 0x%llx with generation %d\n",
                        vm->ax, header->generation);
            }
        }

        if (vm->debug_vm) {
            printf("MALC: reused %zu bytes at 0x%llx (segregated list, block size: %zu, class: %d)\n",
                    size, vm->ax, block->size, size_class);
        }
        goto malc_done;
    }

    // No suitable free block - allocate from bump pointer

    // Check for overflow/OOM (safer than pointer arithmetic which can have UB)
    size_t available = (size_t)(vm->heap_end - vm->heap_ptr);
    if (total_size > available) {
        vm->ax = 0;  // Out of memory or would overflow
        if (vm->debug_vm) {
            printf("MALC: out of memory (requested %zu bytes, need %zu total, available %zu)\n",
                    size, total_size, available);
        }
        if (total_size > (size_t)vm->poolsize) {
            // This is a wraparound/overflow case - requested size is impossibly large
            printf("\n========== HEAP ALLOCATION OVERFLOW ==========\n");
            printf("Allocation size exceeds heap capacity!\n");
            printf("Requested size: %zu bytes\n", size);
            printf("Total size:     %zu bytes\n", total_size);
            printf("Heap capacity:  %d bytes\n", vm->poolsize);
            printf("This may indicate integer overflow or corruption.\n");
            printf("=============================================\n");
        }
    } else {
        // Allocate new block with header
        AllocHeader *header = (AllocHeader *)vm->heap_ptr;
        header->size = size;  // Rounded size
        header->requested_size = requested_size;  // Original requested size
        header->magic = 0xDEADBEEF;
        header->freed = 0;
        header->generation = 0;
        header->alloc_pc = vm->text_seg ? (long long)(vm->pc - vm->text_seg) : 0;
        header->type_kind = TY_VOID;  // Default to void* (generic pointer)

        // If heap canaries enabled, write canaries
        if (vm->flags & JCC_HEAP_CANARIES) {
            header->canary = HEAP_CANARY;
            // Write rear canary after user data
            long long *rear_canary = (long long *)((char *)(header + 1) + size);
            *rear_canary = HEAP_CANARY;
        }

        vm->heap_ptr += total_size;
        vm->ax = (long long)(header + 1);  // Return pointer after header

        // Add to sorted array for O(log n) pointer validation
        insert_sorted_allocation(vm, (void *)vm->ax, header);

        // If memory poisoning enabled, fill with 0xCD pattern
        if (vm->flags & JCC_MEMORY_POISONING) {
            memset((void *)vm->ax, 0xCD, size);
        }

        if (vm->debug_vm) {
            printf("MALC: allocated %zu bytes at 0x%llx (bump allocator, total: %zu)\n",
                    size, vm->ax, total_size);
        }
    }

malc_done:
    // If leak detection enabled, track this allocation
    if (vm->flags & JCC_MEMORY_LEAK_DETECT && vm->ax != 0) {
        // Allocate a record (using real malloc, not VM heap!)
        AllocRecord *record = (AllocRecord *)malloc(sizeof(AllocRecord));
        if (record) {
            record->address = (void *)vm->ax;
            record->size = ((AllocHeader *)((char *)vm->ax - sizeof(AllocHeader)))->size;
            record->alloc_pc = (long long)(vm->pc - vm->text_seg);
            record->next = vm->alloc_list;
            vm->alloc_list = record;
        }
    }

    // Heap provenance is now inferred from sorted_allocs (no longer stored separately)

    // If memory tagging enabled, record creation generation in header
    if (vm->flags & JCC_MEMORY_TAGGING && vm->ax != 0) {
        AllocHeader *header = (AllocHeader *)((char *)vm->ax - sizeof(AllocHeader));
        header->creation_generation = header->generation;

        if (vm->debug_vm) {
            printf("MALC: tagged pointer 0x%llx with generation %d\n",
                    vm->ax, header->generation);
        }
    }
    return 0;
}

int op_MFRE_fn(JCC *vm) {
    // free: pop pointer from stack, validate, and add to free list
    long long ptr = *vm->sp++;

    if (ptr == 0) {
        // Freeing NULL is a no-op (standard behavior)
        vm->ax = 0;
        if (vm->debug_vm) {
            printf("MFRE: freed NULL pointer (no-op)\n");
        }
        return 0;
    }
    // Get the header (it's right before the returned pointer)
    AllocHeader *header = ((AllocHeader *)ptr) - 1;

    // Validate magic number to detect corruption
    if (header->magic != 0xDEADBEEF) {
        printf("\n========== HEAP CORRUPTION DETECTED ==========\n");
        printf("Invalid magic number in allocation header!\n");
        printf("Expected: 0x%x\n", 0xDEADBEEF);
        printf("Found:    0x%x\n", header->magic);
        printf("Address:  0x%llx\n", ptr);
        printf("This may indicate a double-free or heap corruption.\n");
        printf("============================================\n");
        vm->ax = 0;
        return -1;  // Abort execution
    }

    // If heap canaries enabled, check both canaries
    if (vm->flags & JCC_HEAP_CANARIES) {
        // Check front canary (in header)
        if (header->canary != HEAP_CANARY) {
            printf("\n========== HEAP OVERFLOW DETECTED ==========\n");
            printf("Front canary corrupted!\n");
            printf("Expected: 0x%llx\n", HEAP_CANARY);
            printf("Found:    0x%llx\n", header->canary);
            printf("Address:  0x%llx\n", ptr);
            printf("Size:     %zu bytes\n", header->size);
            printf("Allocated at PC offset: %lld\n", header->alloc_pc);
            printf("A buffer underflow has corrupted memory before this allocation.\n");
            printf("============================================\n");
            return -1;  // Abort execution
        }

        // Check rear canary (after user data)
        long long *rear_canary = (long long *)((char *)ptr + header->size);
        if (*rear_canary != HEAP_CANARY) {
            printf("\n========== HEAP OVERFLOW DETECTED ==========\n");
            printf("Rear canary corrupted!\n");
            printf("Expected: 0x%llx\n", HEAP_CANARY);
            printf("Found:    0x%llx\n", *rear_canary);
            printf("Address:  0x%llx\n", ptr);
            printf("Size:     %zu bytes\n", header->size);
            printf("Allocated at PC offset: %lld\n", header->alloc_pc);
            printf("A buffer overflow has written past the end of this allocation.\n");
            printf("============================================\n");
            return -1;  // Abort execution
        }
    }

    size_t size = header->size;

    // ALWAYS check for double-free (even if UAF detection is disabled)
    if (header->freed) {
        printf("\n========== DOUBLE-FREE DETECTED ==========\n");
        printf("Attempted to free already-freed memory\n");
        printf("Address:  0x%llx\n", ptr);
        printf("Size:     %zu bytes\n", header->size);
        printf("Allocated at PC offset: %lld\n", header->alloc_pc);
        printf("Generation: %d\n", header->generation);
        printf("=========================================\n");
        return -1;  // Abort execution
    }

    // If leak detection enabled, remove from tracking list
    if (vm->flags & JCC_MEMORY_LEAK_DETECT) {
        AllocRecord **prev = &vm->alloc_list;
        AllocRecord *curr = vm->alloc_list;
        while (curr) {
            if (curr->address == (void *)ptr) {
                *prev = curr->next;
                free(curr);  // Free the record (not VM memory!)
                break;
            }
            prev = &curr->next;
            curr = curr->next;
        }
    }

    // If memory poisoning enabled, fill with 0xDD (dead memory) pattern
    if (vm->flags & JCC_MEMORY_POISONING) {
        memset((void *)ptr, 0xDD, size);
    }

    // ALWAYS mark as freed and increment generation (for double-free detection)
    header->freed = 1;
    header->generation++;

    // If UAF detection or temporal tagging enabled, quarantine the memory (don't reuse)
    // Temporal tagging requires quarantine to prevent address reuse, which would
    // make it impossible to distinguish stale pointers from valid ones
    int quarantine = vm->flags & JCC_UAF_DETECTION || vm->flags & JCC_MEMORY_TAGGING;

    if (quarantine) {
        // Keep memory quarantined:
        // - Keep in sorted_allocs (needed for CHKP validation and generation comparison)
        // - Do NOT add to free list (prevents reuse)
        if (vm->debug_vm) {
            const char *reason = vm->flags & JCC_UAF_DETECTION ? "UAF detection" : "memory tagging";
            printf("MFRE: quarantined %zu bytes at 0x%llx (%s active, gen=%d)\n",
                    size, ptr, reason, header->generation);
        }
    } else {
        // Normal free: remove from sorted array and allow reuse
        remove_sorted_allocation(vm, (void *)ptr);

        // Normal free: add this block to appropriate segregated list for reuse
        // We reuse the memory for the FreeBlock structure
        FreeBlock *block = (FreeBlock *)header;
        block->size = size;

        // Determine which size class this block belongs to
        int size_class = size_to_class(size);

        if (size_class < 11) {  // Small/medium allocation
            // Insert at head of appropriate size class list
            block->next = vm->size_class_lists[size_class];
            vm->size_class_lists[size_class] = block;

            if (vm->debug_vm) {
                printf("MFRE: freed %zu bytes at 0x%llx (class %d, gen=%d)\n",
                        size, ptr, size_class, header->generation);
            }
        } else {  // Large allocation
            // Insert at head of large list
            block->next = vm->large_list;
            vm->large_list = block;

            if (vm->debug_vm) {
                printf("MFRE: freed %zu bytes at 0x%llx (large list, gen=%d)\n",
                        size, ptr, header->generation);
            }
        }

        // Coalesce adjacent free blocks to reduce fragmentation
        // Updated to work with segregated lists
        coalesce_free_blocks(vm);
    }

    vm->ax = 0;
    return 0;
}

int op_MCPY_fn(JCC *vm) {
    // memcpy: pop size, src, dest from stack, copy bytes, return dest in ax
    long long size = *vm->sp++;
    char *src = (char *)*vm->sp++;
    char *dest = (char *)*vm->sp++;

    // Validate size
    if (size < 0) {
        fprintf(stderr, "MCPY: negative size %lld\n", size);
        return -1;
    }
    if (size > 1024*1024*1024) {  // 1GB sanity limit
        fprintf(stderr, "MCPY: unreasonable size %lld (max 1GB)\n", size);
        return -1;
    }

    // Optional: bounds check src and dest if vm-heap enabled
    if (vm->flags & JCC_VM_HEAP && size > 0) {
        // Check if src and dest are valid pointers
        // For now, just ensure they're non-null
        if (!src || !dest) {
            fprintf(stderr, "MCPY: null pointer (src=0x%llx, dest=0x%llx)\n",
                    (long long)src, (long long)dest);
            return -1;
        }
    }

    // Simple byte-by-byte copy (no overlapping check)
    for (long long i = 0; i < size; i++) {
        dest[i] = src[i];
    }

    vm->ax = (long long)dest;  // Return destination pointer
    if (vm->debug_vm) {
        printf("MCPY: copied %lld bytes from 0x%llx to 0x%llx\n",
                size, (long long)src, (long long)dest);
    }
    return 0;
}

int op_REALC_fn(JCC *vm) {
    // realloc: pop new_size, ptr from stack, realloc memory, return new pointer in ax
    long long new_size = *vm->sp++;
    long long old_ptr = *vm->sp++;

    if (old_ptr == 0) {
        // realloc(NULL, size) is equivalent to malloc(size)
        vm->sp--;  // Push new_size back
        *vm->sp = new_size;
        // Simulate MALC - just set ax to new_size and execute MALC logic
        // For simplicity, we'll just allocate new memory here
        if (new_size <= 0) {
            vm->ax = 0;
        } else {
            // Simplified allocation - just push size and "call" MALC
            // We'll inline a simplified version here
            size_t size = (new_size + 7) & ~7;
            size_t canary_overhead = vm->flags & JCC_HEAP_CANARIES ? sizeof(long long) : 0;
            size_t total_size = size + sizeof(AllocHeader) + canary_overhead;
            size_t available = (size_t)(vm->heap_end - vm->heap_ptr);

            if (total_size > available) {
                vm->ax = 0;  // Out of memory
            } else {
                AllocHeader *header = (AllocHeader *)vm->heap_ptr;
                header->size = size;
                header->requested_size = new_size;
                header->magic = 0xDEADBEEF;
                header->freed = 0;
                header->generation = 0;
                header->alloc_pc = vm->text_seg ? (long long)(vm->pc - vm->text_seg) : 0;
                header->type_kind = TY_VOID;

                if (vm->flags & JCC_HEAP_CANARIES) {
                    header->canary = HEAP_CANARY;
                    long long *rear_canary = (long long *)((char *)(header + 1) + size);
                    *rear_canary = HEAP_CANARY;
                }

                vm->heap_ptr += total_size;
                vm->ax = (long long)(header + 1);

                if (vm->flags & JCC_MEMORY_POISONING) {
                    memset((void *)vm->ax, 0xCD, size);
                }
            }
        }
    } else if (new_size == 0) {
        // realloc(ptr, 0) is equivalent to free(ptr) and return NULL
        // Free the old pointer
        AllocHeader *header = ((AllocHeader *)old_ptr) - 1;
        if (header->magic == 0xDEADBEEF && !header->freed) {
            header->freed = 1;
            header->generation++;
            if (vm->flags & JCC_MEMORY_POISONING) {
                memset((void *)old_ptr, 0xDD, header->size);
            }
        }
        vm->ax = 0;
    } else {
        // Actual realloc: allocate new, copy old, free old
        AllocHeader *old_header = ((AllocHeader *)old_ptr) - 1;

        // Validate old pointer
        if (old_header->magic != 0xDEADBEEF) {
            fprintf(stderr, "REALC: invalid pointer 0x%llx\n", old_ptr);
            vm->ax = 0;
        } else if (old_header->freed) {
            fprintf(stderr, "REALC: use-after-free on pointer 0x%llx\n", old_ptr);
            vm->ax = 0;
        } else {
            // Allocate new memory (simplified)
            size_t new_aligned = (new_size + 7) & ~7;
            size_t canary_overhead = vm->flags & JCC_HEAP_CANARIES ? sizeof(long long) : 0;
            size_t total_size = new_aligned + sizeof(AllocHeader) + canary_overhead;
            size_t available = (size_t)(vm->heap_end - vm->heap_ptr);

            if (total_size > available) {
                vm->ax = 0;  // Out of memory, old pointer unchanged
            } else {
                // Allocate new block
                AllocHeader *new_header = (AllocHeader *)vm->heap_ptr;
                new_header->size = new_aligned;
                new_header->requested_size = new_size;
                new_header->magic = 0xDEADBEEF;
                new_header->freed = 0;
                new_header->generation = 0;
                new_header->alloc_pc = vm->text_seg ? (long long)(vm->pc - vm->text_seg) : 0;
                new_header->type_kind = TY_VOID;

                if (vm->flags & JCC_HEAP_CANARIES) {
                    new_header->canary = HEAP_CANARY;
                    long long *rear_canary = (long long *)((char *)(new_header + 1) + new_aligned);
                    *rear_canary = HEAP_CANARY;
                }

                vm->heap_ptr += total_size;
                long long new_ptr = (long long)(new_header + 1);

                // Copy old data to new location
                size_t copy_size = (old_header->size < new_aligned) ? old_header->size : new_aligned;
                memcpy((void *)new_ptr, (void *)old_ptr, copy_size);

                // Free old pointer
                old_header->freed = 1;
                old_header->generation++;

                if (vm->flags & JCC_MEMORY_POISONING) {
                    memset((void *)old_ptr, 0xDD, old_header->size);
                }

                // New pointer already tracked in sorted_allocs
                vm->ax = new_ptr;

                if (vm->debug_vm) {
                    printf("REALC: reallocated from 0x%llx (%zu bytes) to 0x%llx (%zu bytes)\n",
                           old_ptr, old_header->size, new_ptr, new_aligned);
                }
            }
        }
    }
    return 0;
}

int op_CALC_fn(JCC *vm) {
    // calloc: pop size, count from stack, allocate zeroed memory, return pointer in ax
    // Stack has [count][size] (size on top), so pop size first, then count
    long long elem_size = *vm->sp++;
    long long count = *vm->sp++;

    if (count <= 0 || elem_size <= 0) {
        vm->ax = 0;  // Return NULL for invalid size
    } else {
        // Check for overflow in count * elem_size
        if (count > (1LL << 32) / elem_size) {
            fprintf(stderr, "CALC: size overflow (count=%lld, elem_size=%lld)\n", count, elem_size);
            vm->ax = 0;
        } else {
            long long total = count * elem_size;
            size_t size = (total + 7) & ~7;
            size_t canary_overhead = vm->flags & JCC_HEAP_CANARIES ? sizeof(long long) : 0;
            size_t alloc_size = size + sizeof(AllocHeader) + canary_overhead;
            size_t available = (size_t)(vm->heap_end - vm->heap_ptr);

            if (alloc_size > available) {
                vm->ax = 0;  // Out of memory
            } else {
                AllocHeader *header = (AllocHeader *)vm->heap_ptr;
                header->size = size;
                header->requested_size = total;
                header->magic = 0xDEADBEEF;
                header->freed = 0;
                header->generation = 0;
                header->alloc_pc = vm->text_seg ? (long long)(vm->pc - vm->text_seg) : 0;
                header->type_kind = TY_VOID;

                if (vm->flags & JCC_HEAP_CANARIES) {
                    header->canary = HEAP_CANARY;
                    long long *rear_canary = (long long *)((char *)(header + 1) + size);
                    *rear_canary = HEAP_CANARY;
                }

                vm->heap_ptr += alloc_size;
                vm->ax = (long long)(header + 1);

                // Zero the memory (calloc's defining characteristic)
                memset((void *)vm->ax, 0, size);

                // Allocation already tracked in sorted_allocs

                if (vm->debug_vm) {
                    printf("CALC: allocated %zu bytes (%lld x %lld) at 0x%llx (zeroed)\n",
                            size, count, elem_size, vm->ax);
                }
            }
        }
    }
    return 0;
}

int op_SX1_fn(JCC *vm) {
    vm->ax = (long long)(signed char)(vm->ax & 0xFF);
    return 0;
}

int op_SX2_fn(JCC *vm) {
    vm->ax = (long long)(short)(vm->ax & 0xFFFF);
    return 0;
}

int op_SX4_fn(JCC *vm) {
    vm->ax = (long long)(int)(vm->ax & 0xFFFFFFFF);
    return 0;
}

int op_ZX1_fn(JCC *vm) {
    vm->ax = vm->ax & 0xFF;
    return 0;
}

int op_ZX2_fn(JCC *vm) {
    vm->ax = vm->ax & 0xFFFF;
    return 0;
}

int op_ZX4_fn(JCC *vm) {
    vm->ax = vm->ax & 0xFFFFFFFF;
    return 0;
}

int op_FLD_fn(JCC *vm) {
    vm->fax = *(double *)vm->ax;
    return 0;
}

int op_FST_fn(JCC *vm) {
    *(double *)*vm->sp++ = vm->fax;
    return 0;
}

int op_FADD_fn(JCC *vm) {
    vm->fax = *(double *)vm->sp++ + vm->fax;
    return 0;
}

int op_FSUB_fn(JCC *vm) {
    vm->fax = *(double *)vm->sp++ - vm->fax;
    return 0;
}

int op_FMUL_fn(JCC *vm) {
    vm->fax = *(double *)vm->sp++ * vm->fax;
    return 0;
}

int op_FDIV_fn(JCC *vm) {
    vm->fax = *(double *)vm->sp++ / vm->fax;
    return 0;
}

int op_FNEG_fn(JCC *vm) {
    vm->fax = -vm->fax;
    return 0;
}

int op_FEQ_fn(JCC *vm) {
    vm->ax = *(double *)vm->sp++ == vm->fax;
    return 0;
}

int op_FNE_fn(JCC *vm) {
    vm->ax = *(double *)vm->sp++ != vm->fax;
    return 0;
}

int op_FLT_fn(JCC *vm) {
    vm->ax = *(double *)vm->sp++ < vm->fax;
    return 0;
}

int op_FLE_fn(JCC *vm) {
    vm->ax = *(double *)vm->sp++ <= vm->fax;
    return 0;
}

int op_FGT_fn(JCC *vm) {
    vm->ax = *(double *)vm->sp++ > vm->fax;
    return 0;
}

int op_FGE_fn(JCC *vm) {
    vm->ax = *(double *)vm->sp++ >= vm->fax;
    return 0;
}

int op_I2F_fn(JCC *vm) {
    vm->fax = (double)vm->ax;
    return 0;
}

int op_F2I_fn(JCC *vm) {
    vm->ax = (long long)vm->fax;
    return 0;
}

int op_FPUSH_fn(JCC *vm) {
    *--vm->sp = *(long long *)&vm->fax;
    return 0;
}

int op_CALLF_fn(JCC *vm) {
    // Foreign function call: ax contains FFI function index
    // For variadic functions, actual arg count is on stack (pushed by codegen)
    int func_idx = vm->ax;
    if (func_idx < 0 || func_idx >= vm->compiler.ffi_count) {
        printf("error: invalid FFI function index: %d\n", func_idx);
        return -1;
    }

    ForeignFunc *ff = &vm->compiler.ffi_table[func_idx];

    // Pop actual argument count and double_arg_mask from stack (pushed by codegen for all FFI calls)
    // Stack layout: [args...] [double_arg_mask] [arg_count] [func_idx in ax]
    int actual_nargs = (int)*vm->sp++;
    uint64_t double_arg_mask = (uint64_t)*vm->sp++;  // Bitmask indicating which args are doubles

    if (vm->debug_vm)
        printf("CALLF: calling %s with %d args (fixed: %d, variadic: %d, double_mask: 0x%llx)\n",
                ff->name, actual_nargs, ff->num_fixed_args, ff->is_variadic, double_arg_mask);

    // Format string validation for printf-family functions
    if (vm->flags & JCC_FORMAT_STR_CHECKS) {
        // Check if this is a printf-family function
        // Note: JCC creates specialized variants like printf0, printf1, etc.
        // for different call sites, so we need to check name prefixes
        int is_printf_family = 0;
        int format_arg_index = -1;
        int fixed_args_before_variadic = 0;

        if (strncmp(ff->name, "printf", 6) == 0) {
            is_printf_family = 1;
            format_arg_index = 0;  // First argument is format string
            fixed_args_before_variadic = 1;  // format is fixed
        } else if (strncmp(ff->name, "sprintf", 7) == 0) {
            is_printf_family = 1;
            format_arg_index = 1;  // Second argument is format string (after buffer)
            fixed_args_before_variadic = 2;  // buffer and format are fixed
        } else if (strncmp(ff->name, "snprintf", 8) == 0) {
            is_printf_family = 1;
            format_arg_index = 2;  // Third argument for snprintf (buf, size, format)
            fixed_args_before_variadic = 3;
        } else if (strncmp(ff->name, "fprintf", 7) == 0) {
            is_printf_family = 1;
            format_arg_index = 1;  // Second argument is format string (after FILE*)
            fixed_args_before_variadic = 2;  // stream and format are fixed
        } else if (strncmp(ff->name, "scanf", 5) == 0) {
            is_printf_family = 1;
            format_arg_index = 0;  // First argument is format string
            fixed_args_before_variadic = 1;  // format is fixed
        } else if (strncmp(ff->name, "sscanf", 6) == 0) {
            is_printf_family = 1;
            format_arg_index = 1;  // Second argument is format string (after string)
            fixed_args_before_variadic = 2;  // string and format are fixed
        } else if (strncmp(ff->name, "fscanf", 6) == 0) {
            is_printf_family = 1;
            format_arg_index = 1;  // Second argument is format string (after FILE*)
            fixed_args_before_variadic = 2;  // stream and format are fixed
        }

        if (is_printf_family && format_arg_index >= 0 && format_arg_index < actual_nargs) {
            // After popping actual_nargs, sp points to the first argument
            // Arguments are at sp[0], sp[1], sp[2], ... (in the order they were declared)
            long long format_str_addr = vm->sp[format_arg_index];
            const char *format_str = (const char *)format_str_addr;

            // Validate format string pointer is readable
            // For simplicity, we'll just check if it's not NULL
            if (format_str) {
                int expected_args = count_format_specifiers(format_str);
                int variadic_args = actual_nargs - fixed_args_before_variadic;

                if (expected_args >= 0 && expected_args != variadic_args) {
                    printf("\n========== FORMAT STRING MISMATCH ==========\n");
                    printf("Function:     %s\n", ff->name);
                    printf("Format string: \"%s\"\n", format_str);
                    printf("Expected %d variadic argument(s) from format string\n", expected_args);
                    printf("Received %d variadic argument(s) (total: %d, fixed: %d)\n",
                            variadic_args, actual_nargs, fixed_args_before_variadic);
                    printf("===========================================\n");
                    return -1;
                }
            }
        }
    }

#ifdef JCC_HAS_FFI
    // libffi implementation - supports variadic functions
    // For variadic functions, we need to prepare cif dynamically per call
    // For non-variadic functions, we can cache the cif

    ffi_cif cif;
    ffi_type **arg_types = NULL;
    ffi_type *return_type = ff->returns_double ? &ffi_type_double : &ffi_type_sint64;

    if (ff->is_variadic) {
        // Variadic function - prepare cif dynamically for this specific call
        // Allocate temporary arg_types array
        arg_types = malloc(actual_nargs * sizeof(ffi_type *));
        if (!arg_types) {
            printf("error: failed to allocate arg types for variadic FFI\n");
            return -1;
        }

        // Use double_arg_mask to set correct types
        for (int i = 0; i < actual_nargs; i++) {
            if (i < 64 && (double_arg_mask & (1ULL << i))) {
                arg_types[i] = &ffi_type_double;
            } else {
                arg_types[i] = &ffi_type_sint64;
            }
        }

        // Prepare variadic cif with actual argument count
        ffi_status status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, ff->num_fixed_args,
                                            actual_nargs, return_type, arg_types);
        if (status != FFI_OK) {
            printf("error: failed to prepare variadic FFI cif (status=%d)\n", status);
            free(arg_types);
            return -1;
        }
    } else {
        // Non-variadic function - need to prepare cif for each call since double_arg_mask may vary
        // (We can't cache since codegen double_arg_mask might differ from registered value)
        arg_types = malloc(actual_nargs * sizeof(ffi_type *));
        if (!arg_types) {
            printf("error: failed to allocate arg types for FFI\n");
            return -1;
        }

        // Use double_arg_mask to set correct types
        for (int i = 0; i < actual_nargs; i++) {
            if (i < 64 && (double_arg_mask & (1ULL << i))) {
                arg_types[i] = &ffi_type_double;
            } else {
                arg_types[i] = &ffi_type_sint64;
            }
        }

        ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, actual_nargs,
                                        return_type, arg_types);
        if (status != FFI_OK) {
            printf("error: failed to prepare FFI cif (status=%d)\n", status);
            free(arg_types);
            return -1;
        }
    }

    // Pop arguments from stack (they were pushed right-to-left)
    void *args[actual_nargs];
    long long arg_values[actual_nargs];

    for (int i = 0; i < actual_nargs; i++) {
        arg_values[i] = *vm->sp++;
        args[i] = &arg_values[i];
        if (vm->debug_vm)
            printf("  arg[%d] = 0x%llx (%lld)\n", i, arg_values[i], arg_values[i]);
    }

    // Call the function using libffi
    if (ff->returns_double) {
        double result;
        ffi_call(&cif, FFI_FN(ff->func_ptr), &result, args);
        vm->fax = result;
    } else {
        long long result;
        ffi_call(&cif, FFI_FN(ff->func_ptr), &result, args);
        vm->ax = result;
    }

    // Free temporary arg_types
    if (arg_types) {
        free(arg_types);
    }
#else
    // Fallback implementation without libffi - no variadic support
#if defined(__aarch64__) || defined(__arm64__)
    // ARM64 inline assembly implementation (AAPCS64 calling convention)
    // Supports both non-variadic and variadic functions

    // Pop arguments from VM stack into local array
    long long args[actual_nargs];
    for (int i = 0; i < actual_nargs; i++) {
        args[i] = *vm->sp++;
        if (vm->debug_vm)
            printf("  arg[%d] = 0x%llx (%lld)\n", i, args[i], args[i]);
    }

    // Determine which arguments go in registers vs stack
    // AAPCS64 rules (Darwin/macOS variant):
    // - Fixed integer args: x0-x7
    // - Fixed double args: d0-d7
    // - Variadic args: ALWAYS STACK (on macOS/Darwin)
    // - Remaining args: stack

    int num_fixed = ff->is_variadic ? ff->num_fixed_args : actual_nargs;
    int int_reg_idx = 0;   // Track next available x register (x0-x7)
    int fp_reg_idx = 0;    // Track next available d register (d0-d7)
    int stack_args = 0;    // Count of arguments that go on stack

    // Calculate how many args will go on stack
    for (int i = 0; i < actual_nargs; i++) {
        int is_double = (i < 64 && (double_arg_mask & (1ULL << i)));
        int is_variadic_arg = (i >= num_fixed);

        // On macOS ARM64, ALL variadic arguments go on the stack
        if (is_variadic_arg) {
            stack_args++;
        } else {
            // Fixed arg: use appropriate register type
            if (is_double) {
                if (fp_reg_idx < 8) {
                    fp_reg_idx++;
                } else {
                    stack_args++;
                }
            } else {
                if (int_reg_idx < 8) {
                    int_reg_idx++;
                } else {
                    stack_args++;
                }
            }
        }
    }

    // Allocate stack space locally to hold the values before copying to actual stack
    // We need this because we can't easily iterate args inside inline asm
    long long stack_area[stack_args > 0 ? stack_args : 1];
    int stack_idx = 0;

    // Reset register indices for actual assignment
    int_reg_idx = 0;
    fp_reg_idx = 0;

    // Declare ARM64 register variables
    register long long x0 __asm__("x0") = 0;
    register long long x1 __asm__("x1") = 0;
    register long long x2 __asm__("x2") = 0;
    register long long x3 __asm__("x3") = 0;
    register long long x4 __asm__("x4") = 0;
    register long long x5 __asm__("x5") = 0;
    register long long x6 __asm__("x6") = 0;
    register long long x7 __asm__("x7") = 0;
    register double d0 __asm__("d0") = 0.0;
    register double d1 __asm__("d1") = 0.0;
    register double d2 __asm__("d2") = 0.0;
    register double d3 __asm__("d3") = 0.0;
    register double d4 __asm__("d4") = 0.0;
    register double d5 __asm__("d5") = 0.0;
    register double d6 __asm__("d6") = 0.0;
    register double d7 __asm__("d7") = 0.0;

    for (int i = 0; i < actual_nargs; i++) {
        int is_double = (i < 64 && (double_arg_mask & (1ULL << i)));
        int is_variadic_arg = (i >= num_fixed);

        if (is_variadic_arg) {
            // Variadic arg: always stack on macOS
            stack_area[stack_idx++] = args[i];
        } else {
            // Fixed arg: use appropriate register
            if (is_double) {
                // Extract double from args
                double val = *(double*)&args[i];
                if (fp_reg_idx < 8) {
                    switch(fp_reg_idx++) {
                        case 0: d0 = val; break;
                        case 1: d1 = val; break;
                        case 2: d2 = val; break;
                        case 3: d3 = val; break;
                        case 4: d4 = val; break;
                        case 5: d5 = val; break;
                        case 6: d6 = val; break;
                        case 7: d7 = val; break;
                    }
                } else {
                    stack_area[stack_idx++] = args[i];
                }
            } else {
                // Integer argument
                if (int_reg_idx < 8) {
                    switch(int_reg_idx++) {
                        case 0: x0 = args[i]; break;
                        case 1: x1 = args[i]; break;
                        case 2: x2 = args[i]; break;
                        case 3: x3 = args[i]; break;
                        case 4: x4 = args[i]; break;
                        case 5: x5 = args[i]; break;
                        case 6: x6 = args[i]; break;
                        case 7: x7 = args[i]; break;
                    }
                } else {
                    stack_area[stack_idx++] = args[i];
                }
            }
        }
    }

    // Calculate aligned stack size (must be 16-byte aligned)
    // stack_args is number of 8-byte words
    int stack_bytes = (stack_args * 8 + 15) & ~15;

    // Make the call using inline assembly
    // We need to:
    // 1. Adjust SP to make room for arguments
    // 2. Copy arguments from stack_area to SP
    // 3. Call function
    // 4. Restore SP

    if (ff->returns_double) {
        register double result __asm__("d0");
        __asm__ volatile(
            // Subtract stack space
            "sub sp, sp, %2\n\t"
            // Copy arguments if any
            "cbz %2, 2f\n\t"  // Skip if stack_bytes is 0
            "mov x10, sp\n\t" // Dest
            "mov x11, %3\n\t" // Source (stack_area)
            "mov x12, %4\n\t" // Count (stack_args)
            "1:\n\t"
            "ldr x13, [x11], #8\n\t"
            "str x13, [x10], #8\n\t"
            "subs x12, x12, #1\n\t"
            "b.ne 1b\n\t"
            "2:\n\t"
            "blr %1\n\t"
            // Restore SP
            "add sp, sp, %2"
            : "=r"(result)
            : "r"(ff->func_ptr), "r"((long long)stack_bytes), "r"(stack_area), "r"((long long)stack_args),
              "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6), "r"(x7),
              "w"(d0), "w"(d1), "w"(d2), "w"(d3), "w"(d4), "w"(d5), "w"(d6), "w"(d7)
            : "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18",
              "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15",
              "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
              "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31",
              "memory"
        );
        vm->fax = result;
    } else {
        register long long result __asm__("x0");
        __asm__ volatile(
            // Subtract stack space
            "sub sp, sp, %2\n\t"
            // Copy arguments if any
            "cbz %2, 2f\n\t"  // Skip if stack_bytes is 0
            "mov x10, sp\n\t" // Dest
            "mov x11, %3\n\t" // Source (stack_area)
            "mov x12, %4\n\t" // Count (stack_args)
            "1:\n\t"
            "ldr x13, [x11], #8\n\t"
            "str x13, [x10], #8\n\t"
            "subs x12, x12, #1\n\t"
            "b.ne 1b\n\t"
            "2:\n\t"
            "blr %1\n\t"
            // Restore SP
            "add sp, sp, %2"
            : "=r"(result)
            : "r"(ff->func_ptr), "r"((long long)stack_bytes), "r"(stack_area), "r"((long long)stack_args),
              "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6), "r"(x7),
              "w"(d0), "w"(d1), "w"(d2), "w"(d3), "w"(d4), "w"(d5), "w"(d6), "w"(d7)
            : "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18",
              "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15",
              "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
              "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31",
              "memory"
        );
        vm->ax = result;
    }
#else
    #error "FFI inline assembly not implemented for this platform. Build with -DJCC_HAS_FFI to use libffi."
#endif
#endif  // JCC_HAS_FFI
    return 0;
}

int op_CHKB_fn(JCC *vm) {
    // Check array bounds
    // Stack: [base_ptr, index]
    // Operands: element_size (in *pc)
    if (!(vm->flags & JCC_BOUNDS_CHECKS)) {
        // Bounds checking not enabled, skip
        (void)*vm->pc++;  // Consume operand but don't use it
        return 0;
    }

    long long element_size = *vm->pc++;
    long long index = *vm->sp++;       // Pop index
    long long base_ptr = *vm->sp++;    // Pop base pointer

    // Check for negative index
    if (index < 0) {
        printf("\n========== ARRAY BOUNDS ERROR ==========\n");
        printf("Negative array index: %lld\n", index);
        printf("Base address: 0x%llx\n", base_ptr);
        printf("Element size: %lld bytes\n", element_size);
        printf("PC: 0x%llx (offset: %lld)\n",
                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
        printf("=========================================\n");
        return -1;
    }

    // If this is a heap allocation, check against the allocation size
    if (base_ptr >= (long long)vm->heap_seg && base_ptr < (long long)vm->heap_end) {
        AllocHeader *header = ((AllocHeader *)base_ptr) - 1;

        // Validate magic number
        if (header->magic == 0xDEADBEEF) {
            // Valid heap allocation - check bounds
            long long offset = index * element_size;
            if (offset >= (long long)header->size) {
                printf("\n========== ARRAY BOUNDS ERROR ==========\n");
                printf("Array index out of bounds\n");
                printf("Index:        %lld\n", index);
                printf("Element size: %lld bytes\n", element_size);
                printf("Offset:       %lld bytes\n", offset);
                printf("Array size:   %zu bytes\n", header->size);
                printf("Base address: 0x%llx\n", base_ptr);
                printf("Allocated at PC offset: %lld\n", header->alloc_pc);
                printf("PC: 0x%llx (offset: %lld)\n",
                        (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                printf("=========================================\n");
                return -1;
            }
        }
    }

    // Push values back for the actual array access
    *--vm->sp = base_ptr;
    *--vm->sp = index;

    return 0;
}

int op_CHKP_fn(JCC *vm) {
    // Check pointer validity (for UAF detection, bounds checking, memory tagging)
    // ax contains the pointer to check
    long long ptr = vm->ax;

    if (!(vm->flags & JCC_POINTER_CHECKS)) {
        // No pointer checking enabled, skip
        vm->pc++;
        return 0;
    }

    if (ptr == 0) {
        // NULL pointer is always invalid for dereferencing
        printf("\n========== NULL POINTER DEREFERENCE ==========\n");
        printf("Attempted to dereference NULL pointer\n");
        printf("PC: 0x%llx (offset: %lld)\n",
                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
        printf("============================================\n");
        return -1;
    }

    // Check for dangling stack pointer
    if (vm->flags & JCC_DANGLING_DETECT) {
        if (vm->debug_vm) {
            printf("CHKP: checking pointer 0x%llx (buckets=%p, capacity=%d)\n",
                    ptr, (void*)vm->stack_ptrs.buckets, vm->stack_ptrs.capacity);
        }

        if (vm->stack_ptrs.buckets) {
            // Use integer key API to avoid snprintf overhead
            void *val = hashmap_get_int(&vm->stack_ptrs, ptr);

            if (vm->debug_vm) {
                printf("CHKP: hashmap lookup returned %p\n", val);
            }

            if (val) {
                StackPtrInfo *info = (StackPtrInfo *)val;
                if (info->bp == -1) {
                    // This stack pointer has been invalidated
                    printf("\n========== DANGLING STACK POINTER ==========\n");
                    printf("Attempted to dereference invalidated stack pointer\n");
                    printf("Address:       0x%llx\n", ptr);
                    printf("Original BP:   invalidated (function has returned)\n");
                    printf("Stack offset:  %lld\n", info->offset);
                    printf("Size:          %zu bytes\n", info->size);
                    printf("Current PC:    0x%llx (offset: %lld)\n",
                            (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                    printf("==========================================\n");
                    return -1;
                }
            }
        }
    }

    // Check temporal memory tagging (generation mismatch)
    if (vm->flags & JCC_MEMORY_TAGGING && ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
        // Find allocation using binary search
        int alloc_index = find_containing_allocation(vm, (void *)ptr);

        if (alloc_index >= 0) {
            AllocHeader *header = vm->sorted_allocs.headers[alloc_index];

            // Validate header magic
            if (header && header->magic == 0xDEADBEEF) {
                // Check if creation generation differs from current generation
                // This indicates use-after-free where memory was freed and reallocated
                int creation_generation = header->creation_generation;

                if (creation_generation != header->generation) {
                    printf("\n========== TEMPORAL SAFETY VIOLATION ==========\n");
                    printf("Pointer references memory from a different allocation generation\n");
                    printf("Address:            0x%llx\n", ptr);
                    printf("Pointer tag:        %d (creation generation)\n", creation_generation);
                    printf("Current generation: %d (memory was freed and reallocated)\n", header->generation);
                    printf("Size:               %zu bytes\n", header->size);
                    printf("Allocated at PC offset: %lld\n", header->alloc_pc);
                    printf("Current PC:         0x%llx (offset: %lld)\n",
                            (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                    printf("This indicates use-after-free where memory was freed and reallocated\n");
                    printf("================================================\n");
                    return -1;
                }

                if (vm->debug_vm) {
                    printf("CHKP: temporal tag valid - ptr 0x%llx, generation %d matches\n",
                            ptr, creation_generation);
                }
            }
        }
    }

    // Check if pointer is in heap range
    if (ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
        // Find the allocation that contains this pointer using binary search
        // O(log n) instead of O(m) where m = HashMap capacity
        int alloc_index = find_containing_allocation(vm, (void *)ptr);
        AllocHeader *found_header = NULL;
        long long base_addr = 0;

        // If found, get header and base address from sorted array
        if (alloc_index >= 0) {
            found_header = vm->sorted_allocs.headers[alloc_index];
            base_addr = (long long)vm->sorted_allocs.addresses[alloc_index];
        }

        if (found_header) {
            // Check if freed (UAF detection)
            if (vm->flags & JCC_UAF_DETECTION && found_header->freed) {
                printf("\n========== USE-AFTER-FREE DETECTED ==========\n");
                printf("Attempted to access freed memory\n");
                printf("Address:     0x%llx\n", ptr);
                printf("Base:        0x%llx\n", base_addr);
                printf("Offset:      %lld bytes\n", ptr - base_addr);
                printf("Size:        %zu bytes\n", found_header->size);
                printf("Allocated at PC offset: %lld\n", found_header->alloc_pc);
                printf("Generation:  %d (freed)\n", found_header->generation);
                printf("Current PC:  0x%llx (offset: %lld)\n",
                        (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                printf("============================================\n");
                return -1;
            }

            // Check bounds (use requested_size, not rounded size)
            if (vm->flags & JCC_BOUNDS_CHECKS) {
                long long offset = ptr - base_addr;
                // Check against the originally requested size, not the rounded allocation size
                if (offset < 0 || offset >= (long long)found_header->requested_size) {
                    printf("\n========== ARRAY BOUNDS ERROR ==========\n");
                    printf("Pointer is outside allocated region\n");
                    printf("Address:       0x%llx\n", ptr);
                    printf("Base:          0x%llx\n", base_addr);
                    printf("Offset:        %lld bytes\n", offset);
                    printf("Requested size: %zu bytes\n", found_header->requested_size);
                    printf("Allocated size: %zu bytes (rounded)\n", found_header->size);
                    printf("Allocated at PC offset: %lld\n", found_header->alloc_pc);
                    printf("Current PC:    0x%llx (offset: %lld)\n",
                            (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                    printf("=========================================\n");
                    return -1;
                }
            }
        }
    }

    // Pointer is valid, continue execution
    // ax is unchanged (still contains the pointer)
    return 0;
}

int op_CHKT_fn(JCC *vm) {
    // Check type on pointer dereference
    // Operand: expected TypeKind
    // ax contains the pointer to check
    if (!(vm->flags & JCC_TYPE_CHECKS)) {
        // Type checking not enabled, skip
        (void)*vm->pc++;  // Consume operand but don't use it
        return 0;
    }

    int expected_type = (int)*vm->pc++;
    long long ptr = vm->ax;

    // Skip check for NULL (will be caught by CHKP if enabled)
    if (ptr == 0) {
        return 0;
    }

    // Skip check for void* (TY_VOID) and generic pointers (TY_PTR)
    // These are universal pointers in C
    if (expected_type == TY_VOID || expected_type == TY_PTR) {
        return 0;
    }

    // Only check heap allocations (we can't track stack variable types at runtime)
    if (ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
        // Find the allocation header using binary search
        // O(log n) instead of O(m) where m = HashMap capacity
        int alloc_index = find_containing_allocation(vm, (void *)ptr);
        AllocHeader *found_header = NULL;

        // If found, get header from sorted array
        if (alloc_index >= 0) {
            found_header = vm->sorted_allocs.headers[alloc_index];
        }

        if (found_header) {
            // Check type match
            int actual_type = found_header->type_kind;

            // Skip if allocation was generic (TY_VOID or TY_PTR)
            if (actual_type != TY_VOID && actual_type != TY_PTR) {
                if (actual_type != expected_type) {
                    // Type mismatch!
                    const char *type_names[] = {
                        "void", "bool", "char", "short", "int", "long",
                        "float", "double", "long double", "enum", "pointer",
                        "function", "array", "vla", "struct", "union"
                    };

                    const char *expected_name = (expected_type >= 0 && expected_type < 16)
                        ? type_names[expected_type] : "unknown";
                    const char *actual_name = (actual_type >= 0 && actual_type < 16)
                        ? type_names[actual_type] : "unknown";

                    printf("\n========== TYPE MISMATCH DETECTED ==========\n");
                    printf("Pointer type mismatch on dereference\n");
                    printf("Address:       0x%llx\n", ptr);
                    printf("Expected type: %s\n", expected_name);
                    printf("Actual type:   %s\n", actual_name);
                    printf("Allocated at PC offset: %lld\n", found_header->alloc_pc);
                    printf("Current PC:    0x%llx (offset: %lld)\n",
                            (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                    printf("============================================\n");
                    return -1;
                }
            }
        }
    }

    return 0;
}

int op_CHKI_fn(JCC *vm) {
    // Check if variable is initialized
    // Operand: stack offset (bp-relative)
    if (!(vm->flags & JCC_UNINIT_DETECTION)) {
        // Uninitialized detection not enabled, skip
        (void)*vm->pc++;  // Consume operand but don't use it
        return 0;
    }

    long long offset = *vm->pc++;

    // Adjust offset for stack canaries if enabled
    if (vm->flags & JCC_STACK_CANARIES && offset < 0) {
        offset -= STACK_CANARY_SLOTS;
    }

    // Create key for HashMap lookup (bp address + offset)
    long long addr = (long long)(vm->bp + offset);

    // Check if variable is initialized
    // Use integer key API to avoid snprintf overhead
    void *init = hashmap_get_int(&vm->init_state, addr);
    if (!init) {
        printf("\n========== UNINITIALIZED VARIABLE READ ==========\n");
        printf("Attempted to read uninitialized variable\n");
        printf("Stack offset: %lld\n", offset);
        printf("Address:      0x%llx\n", addr);
        printf("BP:           0x%llx\n", (long long)vm->bp);
        printf("PC:           0x%llx (offset: %lld)\n",
                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
        printf("================================================\n");
        return -1;
    }

    return 0;
}

int op_MARKI_fn(JCC *vm) {
    // Mark variable as initialized
    // Operand: stack offset (bp-relative)
    if (!(vm->flags & JCC_UNINIT_DETECTION)) {
        // Uninitialized detection not enabled, skip
        (void)*vm->pc++;  // Consume operand but don't use it
        return 0;
    }

    long long offset = *vm->pc++;

    // Adjust offset for stack canaries if enabled
    if (vm->flags & JCC_STACK_CANARIES && offset < 0) {
        offset -= STACK_CANARY_SLOTS;
    }

    // Create key for HashMap (bp address + offset)
    long long addr = (long long)(vm->bp + offset);

    // Mark as initialized (use non-NULL value)
    // Use integer key API to avoid snprintf/strdup overhead
    hashmap_put_int(&vm->init_state, addr, (void*)1);

    return 0;
}

int op_MARKA_fn(JCC *vm) {
    // Mark address for stack pointer tracking (dangling pointer detection)
    // Operands: stack offset (bp-relative), size, and scope_id (three immediate values)
    if (!(vm->flags & JCC_DANGLING_DETECT) && !(vm->flags & JCC_STACK_INSTR)) {
        // Neither dangling nor stack instrumentation enabled, skip
        (void)*vm->pc++;  // Consume offset operand
        (void)*vm->pc++;  // Consume size operand
        (void)*vm->pc++;  // Consume scope_id operand
        return 0;
    }

    long long offset = *vm->pc++;
    size_t size = (size_t)*vm->pc++;
    int scope_id = (int)*vm->pc++;

    // ax contains the pointer value (address of stack variable)
    long long ptr = vm->ax;

    if (vm->debug_vm) {
        printf("MARKA: tracking pointer 0x%llx (bp=0x%llx, offset=%lld, size=%zu, scope=%d)\n",
                ptr, (long long)vm->bp, offset, size, scope_id);
    }

    // Create StackPtrInfo
    StackPtrInfo *info = malloc(sizeof(StackPtrInfo));
    if (!info) {
        fprintf(stderr, "MARKA: failed to allocate StackPtrInfo\n");
        return -1;
    }
    info->bp = (long long)vm->bp;
    info->offset = offset;
    info->size = size;
    info->scope_id = scope_id;

    // Store in HashMap
    // Use integer key API to avoid snprintf/strdup overhead
    hashmap_put_int(&vm->stack_ptrs, ptr, info);

    if (vm->debug_vm) {
        printf("MARKA: stored pointer info (capacity=%d, used=%d)\n",
                vm->stack_ptrs.capacity, vm->stack_ptrs.used);
    }

    return 0;
}

int op_CHKA_fn(JCC *vm) {
    // Check pointer alignment
    // Operand: type size (for alignment check)
    if (!(vm->flags & JCC_ALIGNMENT_CHECKS)) {
        // Alignment checking not enabled, skip
        (void)*vm->pc++;  // Consume operand
        return 0;
    }

    size_t type_size = (size_t)*vm->pc++;
    long long ptr = vm->ax;

    // Skip check for NULL
    if (ptr == 0) {
        return 0;
    }

    // Check alignment (pointer must be multiple of type size)
    if (type_size > 1 && (ptr % type_size) != 0) {
        printf("\n========== ALIGNMENT ERROR ==========\n");
        printf("Pointer is misaligned for type\n");
        printf("Address:       0x%llx\n", ptr);
        printf("Type size:     %zu bytes\n", type_size);
        printf("Required alignment: %zu bytes\n", type_size);
        printf("Current PC:    0x%llx (offset: %lld)\n",
                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
        printf("=====================================\n");
        return -1;
    }

    return 0;
}

int op_CHKPA_fn(JCC *vm) {
    // Check pointer arithmetic (invalid arithmetic detection)
    // Uses provenance tracking to validate result is within bounds
    if (!(vm->flags & JCC_INVALID_ARITH) || !(vm->flags & JCC_PROVENANCE_TRACK)) {
        // Need both features enabled
        vm->pc++;
        return 0;
    }

    long long ptr = vm->ax;  // Result of pointer arithmetic

    // Skip check for NULL
    if (ptr == 0) {
        return 0;
    }

    // First check if this is a heap pointer using sorted_allocs
    if (ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
        int alloc_index = find_containing_allocation(vm, (void *)ptr);
        if (alloc_index >= 0) {
            AllocHeader *header = vm->sorted_allocs.headers[alloc_index];
            long long base = (long long)vm->sorted_allocs.addresses[alloc_index];
            long long end = base + (long long)header->requested_size;

            if (ptr < base || ptr > end) {
                printf("\n========== INVALID POINTER ARITHMETIC ==========\n");
                printf("Pointer arithmetic result outside object bounds\n");
                printf("Origin:        HEAP\n");
                printf("Object base:   0x%llx\n", base);
                printf("Object size:   %zu bytes\n", header->requested_size);
                printf("Result ptr:    0x%llx\n", ptr);
                printf("Offset:        %lld bytes from base\n", ptr - base);
                printf("Current PC:    0x%llx (offset: %lld)\n",
                        (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                printf("===============================================\n");
                return -1;
            }
            return 0;  // Heap pointer validated
        }
    }

    // Not a heap pointer - check provenance HashMap for stack/global pointers
    void *val = hashmap_get_int(&vm->provenance, ptr);

    if (val) {
        ProvenanceInfo *info = (ProvenanceInfo *)val;

        // Check if pointer is within valid range [base, base+size]
        long long end = info->base + (long long)info->size;
        if (ptr < info->base || ptr > end) {
            const char *origin_names[] = {"HEAP", "STACK", "GLOBAL"};
            printf("\n========== INVALID POINTER ARITHMETIC ==========\n");
            printf("Pointer arithmetic result outside object bounds\n");
            printf("Origin:        %s\n", origin_names[info->origin_type]);
            printf("Object base:   0x%llx\n", info->base);
            printf("Object size:   %zu bytes\n", info->size);
            printf("Result ptr:    0x%llx\n", ptr);
            printf("Offset:        %lld bytes from base\n", ptr - info->base);
            printf("Current PC:    0x%llx (offset: %lld)\n",
                    (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
            printf("===============================================\n");
            return -1;
        }
    }

    // If no provenance info, we can't validate (might be a computed pointer)
    return 0;
}

int op_MARKP_fn(JCC *vm) {
    // Mark provenance for pointer tracking
    // Operands: origin_type, base, size (encoded as three immediate values)
    if (!(vm->flags & JCC_PROVENANCE_TRACK)) {
        // Provenance tracking not enabled, skip
        (void)*vm->pc++;  // Consume origin_type operand
        (void)*vm->pc++;  // Consume base operand
        (void)*vm->pc++;  // Consume size operand
        return 0;
    }

    int origin_type = (int)*vm->pc++;  // 0=HEAP, 1=STACK, 2=GLOBAL
    long long base = *vm->pc++;
    size_t size = (size_t)*vm->pc++;

    // ax contains the pointer value
    long long ptr = vm->ax;

    // Create ProvenanceInfo
    ProvenanceInfo *info = malloc(sizeof(ProvenanceInfo));
    info->origin_type = origin_type;
    info->base = base;
    info->size = size;

    // Store in HashMap
    // Use integer key API to avoid snprintf/strdup overhead
    hashmap_put_int(&vm->provenance, ptr, info);

    return 0;
}

int op_SCOPEIN_fn(JCC *vm) {
    // Mark scope entry - activate all variables in this scope
    // Operand: scope_id
    if (!(vm->flags & JCC_STACK_INSTR)) {
        (void)*vm->pc++;  // Consume scope_id operand
        return 0;
    }

    int scope_id = (int)*vm->pc++;

    if (vm->debug_vm) {
        printf("SCOPEIN: entering scope %d (bp=0x%llx)\n", scope_id, (long long)vm->bp);
    }

    // NEW: Iterate only over variables in this scope using the scope list
    if (scope_id < vm->scope_vars_capacity) {
        ScopeVarNode *node = vm->scope_vars[scope_id].head;
        while (node) {
            StackVarMeta *meta = node->meta;
            if (meta) {
                meta->is_alive = 1;
                meta->bp = (long long)vm->bp;
                if (vm->debug_vm) {
                    printf("  Activated variable '%s' at bp%+lld\n", meta->name, meta->offset);
                }
            }
            node = node->next;
        }
    }

    return 0;
}


int op_SCOPEOUT_fn(JCC *vm) {
    // Mark scope exit - deactivate variables and detect dangling pointers
    // Operand: scope_id
    if (!(vm->flags & JCC_STACK_INSTR)) {
        (void)*vm->pc++;  // Consume scope_id operand
        return 0;
    }

    int scope_id = (int)*vm->pc++;

    if (vm->debug_vm) {
        printf("SCOPEOUT: exiting scope %d (bp=0x%llx)\n", scope_id, (long long)vm->bp);
    }

    // NEW: Iterate only over variables in this scope using the scope list
    if (scope_id < vm->scope_vars_capacity) {
        ScopeVarNode *node = vm->scope_vars[scope_id].head;
        while (node) {
            StackVarMeta *meta = node->meta;
            if (meta && meta->bp == (long long)vm->bp) {
                meta->is_alive = 0;
                if (vm->debug_vm) {
                    printf("  Deactivated variable '%s' at bp%+lld (reads=%lld, writes=%lld)\n",
                            meta->name, meta->offset, meta->read_count, meta->write_count);
                }
            }
            node = node->next;
        }
    }


    // Check for dangling pointers (pointers to variables in this scope)
    if (vm->flags & JCC_DANGLING_DETECT) {
        for (int i = 0; i < vm->stack_ptrs.capacity; i++) {
            if (vm->stack_ptrs.buckets[i].key != NULL) {
                StackPtrInfo *ptr_info = (StackPtrInfo *)vm->stack_ptrs.buckets[i].val;
                if (ptr_info && ptr_info->scope_id == scope_id && ptr_info->bp == (long long)vm->bp) {
                    if (vm->flags & JCC_STACK_INSTR_ERRORS) {
                        printf("\n========== DANGLING POINTER DETECTED ==========\n");
                        printf("Pointer to stack variable in scope %d still exists\n", scope_id);
                        printf("Pointer: 0x%s (bp=%+lld)\n", vm->stack_ptrs.buckets[i].key, ptr_info->offset);
                        printf("Scope is now exiting - this pointer will dangle\n");
                        printf("Current PC: 0x%llx (offset: %lld)\n",
                                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                        printf("==============================================\n");
                        return -1;
                    } else if (vm->debug_vm) {
                        printf("WARNING: Dangling pointer detected for scope %d\n", scope_id);
                    }
                }
            }
        }
    }

    return 0;
}

int op_CHKL_fn(JCC *vm) {
    // Check variable liveness before access
    // Operand: offset (bp-relative)
    if (!(vm->flags & JCC_STACK_INSTR)) {
        (void)*vm->pc++;  // Consume offset operand
        return 0;
    }

    long long offset = *vm->pc++;

    // Look up variable metadata
    // Use integer key API to avoid snprintf overhead
    StackVarMeta *meta = (StackVarMeta *)hashmap_get_int(&vm->stack_var_meta, offset);

    if (meta) {
        // Check if variable matches current BP (different frames shouldn't interfere)
        if (meta->bp != (long long)vm->bp && meta->bp != 0) {
            // Different frame - this is use after function return
            if (vm->flags & JCC_STACK_INSTR_ERRORS) {
                printf("\n========== USE AFTER RETURN DETECTED ==========\n");
                printf("Variable '%s' at bp%+lld accessed after function return\n",
                        meta->name, meta->offset);
                printf("Variable BP:  0x%llx\n", meta->bp);
                printf("Current BP:   0x%llx\n", (long long)vm->bp);
                printf("Current PC:   0x%llx (offset: %lld)\n",
                        (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                printf("==============================================\n");
                return -1;
            }
        }

        // Check if variable is alive
        if (!meta->is_alive) {
            if (vm->flags & JCC_STACK_INSTR_ERRORS) {
                printf("\n========== USE AFTER SCOPE DETECTED ==========\n");
                printf("Variable '%s' at bp%+lld accessed after scope exit\n",
                        meta->name, meta->offset);
                printf("Scope ID: %d\n", meta->scope_id);
                printf("Current PC: 0x%llx (offset: %lld)\n",
                        (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                printf("=============================================\n");
                return -1;
            } else if (vm->debug_vm) {
                printf("WARNING: Variable '%s' accessed after scope exit\n", meta->name);
            }
        }
    }

    return 0;
}

int op_MARKR_fn(JCC *vm) {
    // Mark variable read access
    // Operand: offset (bp-relative)
    if (!(vm->flags & JCC_STACK_INSTR)) {
        (void)*vm->pc++;  // Consume offset operand
        return 0;
    }

    long long offset = *vm->pc++;

    // Look up variable metadata
    // Use integer key API to avoid snprintf overhead
    StackVarMeta *meta = (StackVarMeta *)hashmap_get_int(&vm->stack_var_meta, offset);

    if (meta && meta->bp == (long long)vm->bp) {
        meta->read_count++;
        if (vm->debug_vm) {
            printf("MARKR: '%s' read (count=%lld)\n", meta->name, meta->read_count);
        }
    }

    return 0;
}

int op_MARKW_fn(JCC *vm) {
    // Mark variable write access
    // Operand: offset (bp-relative)
    if (!(vm->flags & JCC_STACK_INSTR)) {
        (void)*vm->pc++;  // Consume offset operand
        return 0;
    }

    long long offset = *vm->pc++;

    // Look up variable metadata
    // Use integer key API to avoid snprintf overhead
    StackVarMeta *meta = (StackVarMeta *)hashmap_get_int(&vm->stack_var_meta, offset);

    if (meta && meta->bp == (long long)vm->bp) {
        meta->write_count++;
        // Mark as initialized on first write
        if (!meta->initialized) {
            meta->initialized = 1;
        }
        if (vm->debug_vm) {
            printf("MARKW: '%s' write (count=%lld)\n", meta->name, meta->write_count);
        }
    }

    return 0;
}

int op_SETJMP_fn(JCC *vm) {
    // setjmp: Save execution context to jmp_buf and return 0
    // The jmp_buf address is in ax (not on stack)
    long long *jmp_buf_ptr = (long long *)vm->ax;

    // Save VM state to jmp_buf
    // [0] = pc (return address - where longjmp will jump back to)
    // [1] = sp (stack pointer - current state)
    // [2] = bp (base pointer)
    // [3] = stack value at sp (for restoration)
    // [4] = reserved
    jmp_buf_ptr[0] = (long long)vm->pc;      // Save return address
    jmp_buf_ptr[1] = (long long)vm->sp;      // Save stack pointer
    jmp_buf_ptr[2] = (long long)vm->bp;      // Save base pointer
    jmp_buf_ptr[3] = *vm->sp;                 // Save value at top of stack!
    jmp_buf_ptr[4] = 0;                       // Reserved

    // setjmp returns 0 when called directly
    vm->ax = 0;
    return 0;
}

int op_LONGJMP_fn(JCC *vm) {
    // longjmp: Restore execution context from jmp_buf and return val
    // Stack top-to-bottom: [env, val] (env pushed last, so on top)
    long long *jmp_buf_ptr = (long long *)*vm->sp++;  // Pop jmp_buf pointer (env)
    int val = (int)*vm->sp++;                          // Pop return value

    // Check if jmp_buf_ptr is valid
    if (jmp_buf_ptr == NULL || jmp_buf_ptr == (void*)0) {
        return -1;
    }

    // Restore VM state from jmp_buf
    vm->pc = (long long *)jmp_buf_ptr[0];     // Restore program counter
    vm->sp = (long long *)jmp_buf_ptr[1];     // Restore stack pointer
    vm->bp = (long long *)jmp_buf_ptr[2];     // Restore base pointer

    // Restore the stack value that was saved!
    *vm->sp = jmp_buf_ptr[3];

    // Set return value (convert 0 to 1 per C standard)
    vm->ax = (val == 0) ? 1 : val;
    return 0;
}
