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

#ifndef JCC_NO_THREADS
// Forward declarations for threading support (now in jcc.h)
// Global VM pointer for pthread FFI wrappers (they can't receive vm as parameter)
JCC *__global_vm = NULL;
#endif

// Stack canary constant for detecting stack overflows (used when random canaries disabled)
#define STACK_CANARY 0xDEADBEEFCAFEBABELL

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

#ifndef JCC_NO_THREADS
// ============================================================================
// Global Interpreter Lock (GIL) Implementation
// ============================================================================

/*!
 * @function gil_init
 * @abstract Initialize the Global Interpreter Lock
 */
static void gil_init(GIL *gil) {
    pthread_mutex_init(&gil->lock, NULL);
    gil->owner_thread = 0;
    gil->recursion_count = 0;
}

/*!
 * @function gil_acquire
 * @abstract Acquire the GIL, with support for recursive acquisition
 * @discussion If the current thread already owns the GIL, increment recursion count.
 *             Otherwise, block until GIL is available.
 */
void gil_acquire(GIL *gil) {
    pthread_t self = pthread_self();

    // Check for recursive acquisition
    if (pthread_equal(gil->owner_thread, self)) {
        gil->recursion_count++;
        return;
    }

    // Block until we can acquire the lock
    pthread_mutex_lock(&gil->lock);
    gil->owner_thread = self;
    gil->recursion_count = 1;
}

/*!
 * @function gil_release
 * @abstract Release the GIL, decrementing recursion count
 * @discussion Only actually releases the mutex when recursion count reaches 0
 */
void gil_release(GIL *gil) {
    pthread_t self = pthread_self();
    assert(pthread_equal(gil->owner_thread, self) && "GIL released by non-owner");

    gil->recursion_count--;
    if (gil->recursion_count == 0) {
        gil->owner_thread = 0;
        pthread_mutex_unlock(&gil->lock);
    }
}

/*!
 * @function gil_check_owned
 * @abstract Check if current thread holds the GIL
 * @return 1 if current thread owns GIL, 0 otherwise
 */
static int gil_check_owned(GIL *gil) {
    return pthread_equal(gil->owner_thread, pthread_self());
}

/*!
 * @function gil_destroy
 * @abstract Clean up GIL resources
 */
static void gil_destroy(GIL *gil) {
    pthread_mutex_destroy(&gil->lock);
    gil->owner_thread = 0;
    gil->recursion_count = 0;
}

// ============================================================================
// VM Thread Management
// ============================================================================

/*!
 * @function vm_thread_get_current
 * @abstract Get the current thread's VMThread structure
 * @discussion Uses pthread_getspecific with thread_key. Returns main_thread
 *             if threading is disabled.
 */
VMThread *vm_thread_get_current(JCC *vm) {
    if (!vm->enable_threading) {
        return vm->main_thread;  // Single-threaded mode
    }

    VMThread *thread = (VMThread *)pthread_getspecific(vm->thread_key);
    return thread;
}

/*!
 * @function vm_thread_set_current
 * @abstract Set the current thread's VMThread structure in TLS
 */
void vm_thread_set_current(JCC *vm, VMThread *thread) {
    pthread_setspecific(vm->thread_key, thread);
}

/*!
 * @function vm_thread_allocate
 * @abstract Allocate and initialize a new VMThread structure
 * @param vm The VM instance
 * @param entry_point Bytecode offset of function to execute
 * @param args Array of arguments to pass to function
 * @param num_args Number of arguments
 * @return Pointer to newly allocated VMThread
 */
VMThread *vm_thread_allocate(JCC *vm, long long entry_point, long long *args, int num_args) {
    VMThread *thread = (VMThread *)calloc(1, sizeof(VMThread));
    if (!thread) {
        fprintf(stderr, "Failed to allocate VMThread\n");
        return NULL;
    }

    thread->vm = vm;
    thread->entry_point = entry_point;
    thread->num_args = num_args;
    thread->terminated = 0;
    thread->detached = 0;
    thread->exit_code = 0;
    thread->next = NULL;

    // Allocate thread-local stack (same size as main stack)
    thread->stack_seg = (long long *)calloc(vm->poolsize, 1);
    if (!thread->stack_seg) {
        fprintf(stderr, "Failed to allocate thread stack\n");
        free(thread);
        return NULL;
    }

    // Initialize stack pointers (stack grows downward)
    // poolsize is in bytes, divide by element size to get element count
    size_t stack_elements = vm->poolsize / sizeof(long long);
    thread->sp = thread->stack_seg + stack_elements;
    thread->bp = thread->sp;
    thread->initial_sp = thread->sp;
    thread->initial_bp = thread->bp;

    // Initialize accumulators
    thread->ax = 0;
    thread->fax = 0.0;

    // Allocate TLS segment (for _Thread_local variables)
    thread->tls_seg = (char *)calloc(vm->poolsize / 16, 1);  // 1/16th of main poolsize
    thread->tls_ptr = thread->tls_seg;

    // Copy arguments to thread's stack
    if (num_args > 0 && args) {
        thread->entry_args = (long long *)malloc(num_args * sizeof(long long));
        memcpy(thread->entry_args, args, num_args * sizeof(long long));
    } else {
        thread->entry_args = NULL;
    }

    // Set PC to entry point
    // CRITICAL: entry_point is a byte offset, but text_seg is long long*
    // Must cast to char* to avoid pointer arithmetic multiplying by 8
    thread->pc = (long long *)((char *)vm->text_seg + entry_point);

    return thread;
}

/*!
 * @function init_thread_tls
 * @brief Initialize TLS variables for a thread
 * @param vm VM instance
 * @param thread Thread to initialize TLS for
 *
 * Copies initializers from global _Thread_local variables to the thread's TLS segment.
 */
static void init_thread_tls(JCC *vm, VMThread *thread) {
    if (!thread || !thread->tls_seg) {
        return;
    }

    // Initialize TLS variables for this thread
    for (Obj *var = vm->globals; var; var = var->next) {
        if (var->is_tls) {
            if (var->init_data) {
                // Copy initializer to thread's TLS
                memcpy(thread->tls_seg + var->offset, var->init_data, var->ty->size);
            } else {
                // Zero-initialize (already done by calloc, but be explicit)
                memset(thread->tls_seg + var->offset, 0, var->ty->size);
            }
        }
    }
}

/*!
 * @function vm_thread_destroy
 * @abstract Free all resources associated with a VMThread
 */
void vm_thread_destroy(VMThread *thread) {
    if (!thread) return;

    if (thread->stack_seg) {
        free(thread->stack_seg);
    }
    if (thread->tls_seg) {
        free(thread->tls_seg);
    }
    if (thread->entry_args) {
        free(thread->entry_args);
    }
    free(thread);
}

/*!
 * @function vm_thread_entry
 * @abstract Entry point for new OS threads - executes VM bytecode
 * @param arg Pointer to VMThread structure
 */
void *vm_thread_entry(void *arg) {
    VMThread *thread = (VMThread *)arg;
    JCC *vm = thread->vm;

    // Set thread-local VMThread pointer
    vm_thread_set_current(vm, thread);

    // Set up stack frame for thread entry point
    // The thread starts execution at the function entry point, as if CALL had just been executed.
    // CALL pushes the return address, then the function's ENT instruction pushes saved bp.
    // Stack layout before ENT: [...args...] [return_address] <- sp
    // After ENT: [...args...] [return_address] [saved_bp] [locals...] <- sp
    //
    // We need to push arguments first, then return address (like CALL would have done).
    // When the function returns (LEV), it will pop bp and return address.
    // Return address = NULL signals clean exit.

    // Push arguments onto thread's stack (right-to-left)
    for (int i = thread->num_args - 1; i >= 0; i--) {
        *--thread->sp = thread->entry_args[i];
    }

    // Push return address (NULL signals thread exit)
    *--thread->sp = 0;

    // Acquire GIL before executing (if enabled)
    // NOTE: No need to copy state to vm->registers anymore - vm_eval uses thread->registers directly via VM_ macros
    if (vm->enable_gil) {
        gil_acquire(&vm->gil);
    }

    // Execute VM bytecode (vm_eval will use thread->pc/sp/bp/ax/fax via VM_ macros)
    int eval_result = vm_eval(vm);

    // Thread function's return value is in thread->ax (accessed via VM_AX inside vm_eval)
    // vm_eval's return value is the exit code (0 = success, non-zero = error)
    thread->exit_code = (eval_result == 0) ? thread->ax : eval_result;
    thread->terminated = 1;

    fprintf(stderr, "DEBUG vm_thread_entry: About to release GIL and return, thread=%p, exit_code=%d\n",
            thread, thread->exit_code);

    // Release GIL (if enabled)
    if (vm->enable_gil) {
        gil_release(&vm->gil);
        fprintf(stderr, "DEBUG vm_thread_entry: GIL released, thread=%p\n", thread);
    }

    fprintf(stderr, "DEBUG vm_thread_entry: Returning NULL, thread=%p\n", thread);
    return NULL;
}
#endif // JCC_NO_THREADS

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
    if (!vm->enable_memory_leak_detection)
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
// Helper function to remove a block from any segregated list
static int remove_from_segregated_lists(JCC *vm, FreeBlock *target) {
    // Search through all size class lists
    for (int i = 0; i < NUM_SIZE_CLASSES - 1; i++) {
        FreeBlock **prev = &vm->size_class_lists[i];
        FreeBlock *curr = vm->size_class_lists[i];

        while (curr) {
            if (curr == target) {
                *prev = curr->next;
                return i;  // Return the size class it was found in
            }
            prev = &curr->next;
            curr = curr->next;
        }
    }

    // Search large list
    FreeBlock **prev = &vm->large_list;
    FreeBlock *curr = vm->large_list;
    while (curr) {
        if (curr == target) {
            *prev = curr->next;
            return NUM_SIZE_CLASSES - 1;  // Large class
        }
        prev = &curr->next;
        curr = curr->next;
    }

    return -1;  // Not found
}

// Coalesce adjacent free blocks across all segregated lists
// This is more complex than single-list coalescing because blocks can be in different size classes
static void coalesce_free_blocks(JCC *vm) {
    size_t canary_overhead = vm->enable_heap_canaries ? sizeof(long long) : 0;
    int coalesced = 0;

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

    // Try to merge adjacent blocks
    for (int i = 0; i < block_count; i++) {
        if (!all_blocks[i]) continue;  // Already merged

        FreeBlock *block1 = all_blocks[i];
        char *block1_start = (char *)block1;
        char *block1_end = block1_start + sizeof(AllocHeader) + block1->size + canary_overhead;

        for (int j = i + 1; j < block_count; j++) {
            if (!all_blocks[j]) continue;

            FreeBlock *block2 = all_blocks[j];
            char *block2_start = (char *)block2;
            char *block2_end = block2_start + sizeof(AllocHeader) + block2->size + canary_overhead;

            // Check if blocks are adjacent
            if (block1_end == block2_start) {
                // block1 is immediately before block2 - merge block2 into block1
                remove_from_segregated_lists(vm, block2);
                block1->size += sizeof(AllocHeader) + block2->size + canary_overhead;
                all_blocks[j] = NULL;  // Mark as merged
                block1_end = block1_start + sizeof(AllocHeader) + block1->size + canary_overhead;
                coalesced = 1;

                if (vm->debug_vm) {
                    printf("COALESCE: merged adjacent blocks (new size: %zu bytes)\n", block1->size);
                }
            } else if (block2_end == block1_start) {
                // block2 is immediately before block1 - merge block1 into block2
                remove_from_segregated_lists(vm, block1);
                block2->size += sizeof(AllocHeader) + block1->size + canary_overhead;
                all_blocks[i] = block2;  // Update reference
                all_blocks[j] = NULL;     // Mark block2's old position
                block1 = block2;
                block1_start = (char *)block1;
                block1_end = block1_start + sizeof(AllocHeader) + block1->size + canary_overhead;
                coalesced = 1;

                if (vm->debug_vm) {
                    printf("COALESCE: merged adjacent blocks (new size: %zu bytes)\n", block2->size);
                }
            }
        }

        // After potential merges, re-insert block1 into appropriate size class
        if (coalesced && all_blocks[i]) {
            int new_class = size_to_class(all_blocks[i]->size);
            if (new_class < NUM_SIZE_CLASSES - 1) {
                all_blocks[i]->next = vm->size_class_lists[new_class];
                vm->size_class_lists[new_class] = all_blocks[i];
            } else {
                all_blocks[i]->next = vm->large_list;
                vm->large_list = all_blocks[i];
            }
        }
    }
}

int vm_eval(JCC *vm) {
    int op;

    // Get current thread context (NULL for compile-time/single-threaded execution)
    VMThread *thread = NULL;
    VMThread temp_thread_storage;
    int using_temp_thread = 0;

    #ifndef JCC_NO_THREADS
    if (vm->enable_threading) {
        thread = vm_thread_get_current(vm);

        // Sanity check: thread should never be NULL when threading is enabled
        if (!thread) {
            fprintf(stderr, "FATAL: vm_thread_get_current returned NULL with threading enabled!\n");
            return -1;
        }

        fprintf(stderr, "DEBUG vm_eval START: thread=%p, thread->pc=%p\n", thread, thread->pc);
    }
    #endif

    // If no thread context (compile-time/single-threaded), create temporary context
    if (!thread) {
        temp_thread_storage.pc = vm->pc;
        temp_thread_storage.sp = vm->sp;
        temp_thread_storage.bp = vm->bp;
        temp_thread_storage.ax = vm->ax;
        temp_thread_storage.fax = vm->fax;
        thread = &temp_thread_storage;
        using_temp_thread = 1;
    }

    // Register accessor macros - automatically use thread or vm registers based on build config
    #ifndef JCC_NO_THREADS
        #define VM_PC   thread->pc
        #define VM_SP   thread->sp
        #define VM_BP   thread->bp
        #define VM_AX   thread->ax
        #define VM_FAX  thread->fax
    #else
        #define VM_PC   vm->pc
        #define VM_SP   vm->sp
        #define VM_BP   vm->bp
        #define VM_AX   vm->ax
        #define VM_FAX  vm->fax
    #endif

    // Helper macro to copy temp thread state back before returning
    #define VM_RETURN(val) do { \
        if (using_temp_thread) { \
            vm->pc = thread->pc; \
            vm->sp = thread->sp; \
            vm->bp = thread->bp; \
            vm->ax = thread->ax; \
            vm->fax = thread->fax; \
        } \
        return (val); \
    } while(0)

    vm->cycle = 0;
    while (1) {
        vm->cycle++;

        // DEBUG: Print VM_PC before dereferencing (only when NULL)
        if (VM_PC == NULL) {
            fprintf(stderr, "FATAL cycle %lld: VM_PC is NULL! thread=%p\n", vm->cycle, thread);
            VM_RETURN(-1);
        }

        // Debugger hooks - check before executing instruction
        if (vm->enable_debugger) {
            // Check for breakpoints
            if (debugger_check_breakpoint(vm)) {
                printf("\nBreakpoint hit at PC %p (offset: %lld)\n",
                       (void*)VM_PC, (long long)(VM_PC - vm->text_seg));
                cc_debug_repl(vm);
            }

            if (vm->single_step)
                cc_debug_repl(vm);

            if (vm->step_over && VM_PC == vm->step_over_return_addr) {
                vm->step_over = 0;
                cc_debug_repl(vm);
            }

            if (vm->step_out && VM_BP != vm->step_out_bp) {
                vm->step_out = 0;
                cc_debug_repl(vm);
            }
        }

        op = *VM_PC++; // get next operation code

        // DEBUG: Log operations in cycles 7-10
        if (vm->cycle >= 7 && vm->cycle <= 10) {
            fprintf(stderr, "DEBUG cycle %lld: opcode=%d, AX before=0x%llx\n",
                    vm->cycle, op, VM_AX);
        }

        if (vm->debug_vm) {
            printf("%lld> %.5s", vm->cycle,
                    &
                    "LEA  ,IMM  ,JMP  ,CALL ,CALI ,JZ   ,JNZ  ,ENT  ,ADJ  ,LEV  ,LI   ,LC   ,LS   ,LW   ,SI   ,SC   ,SS   ,SW   ,PUSH ,"
                    "OR   ,XOR  ,AND  ,EQ   ,NE   ,LT   ,GT   ,LE   ,GE   ,SHL  ,SHR  ,ADD  ,SUB  ,MUL  ,DIV  ,MOD  ,"
                    "MALC ,MFRE ,MCPY ,"
                    "SX1  ,SX2  ,SX4  ,ZX1  ,ZX2  ,ZX4  ,"
                    "FLD  ,FST  ,FADD ,FSUB ,FMUL ,FDIV ,FNEG ,FEQ  ,FNE  ,FLT  ,FLE  ,FGT  ,FGE  ,I2F  ,F2I  ,FPSH ,"
                    "CALLF,CHKB ,CHKP ,CHKT ,CHKI ,MARKI,MARKA,CHKA ,CHKPA,MARKP,"
                    "SCPIN,SCPOT,CHKL ,MARKR,MARKW,"
                    "SETJP,LONJP,CAS  ,EXCH ,TLSAD"[op * 6]);
            if (op <= ADJ || op == CHKB || op == CHKT || op == CHKI || op == MARKI || op == MARKA || op == CHKA || op == MARKP || op == SCOPEIN || op == SCOPEOUT || op == CHKL || op == MARKR || op == MARKW || op == CAS || op == EXCH || op == TLSADDR)
                printf(" %lld\n", *VM_PC);
            else
                printf("\n");
        }
        
        if (op == IMM)        {
            long long imm_val = *VM_PC++;
            VM_AX = imm_val;
            // DEBUG: Log IMM loads that look like pointers
            if (vm->cycle <= 50 && (imm_val > 0x100000000LL || imm_val == 0x18)) {
                fprintf(stderr, "DEBUG IMM cycle %lld: Loaded 0x%llx from PC-1=%p into AX\n",
                        vm->cycle, imm_val, VM_PC - 1);
            }
        }                                        // load immediate value to ax
        else if (op == LC)    {
            // Check read watchpoints before loading
            if (vm->enable_debugger && vm->num_watchpoints > 0) {
                debugger_check_watchpoint(vm, (void*)VM_AX, 1, WATCH_READ);
            }
            VM_AX = (long long)(*(signed char *)(long long)VM_AX);   // load signed char, sign-extend to long long
        }
        else if (op == LS)    {
            // Check read watchpoints before loading
            if (vm->enable_debugger && vm->num_watchpoints > 0) {
                debugger_check_watchpoint(vm, (void*)VM_AX, 2, WATCH_READ);
            }
            VM_AX = (long long)(*(short *)(long long)VM_AX);         // load short, sign-extend to long long
        }
        else if (op == LW)    {
            // Check read watchpoints before loading
            if (vm->enable_debugger && vm->num_watchpoints > 0) {
                debugger_check_watchpoint(vm, (void*)VM_AX, 4, WATCH_READ);
            }
            VM_AX = (long long)(*(int *)(long long)VM_AX);           // load int, sign-extend to long long
        }
        else if (op == LI)    {
            // Check read watchpoints before loading
            if (vm->enable_debugger && vm->num_watchpoints > 0) {
                debugger_check_watchpoint(vm, (void*)VM_AX, 8, WATCH_READ);
            }
            VM_AX = *(long long *)(long long)VM_AX;                  // load long long to ax, address in ax
        }
        else if (op == SC)    {
            // Check write watchpoints before storing
            if (vm->enable_debugger && vm->num_watchpoints > 0) {
                debugger_check_watchpoint(vm, (void*)*VM_SP, 1, WATCH_WRITE);
            }
            VM_AX = *(char *)*VM_SP++ = VM_AX;                      // save character to address, value in ax, address on stack
        }
        else if (op == SS)    {
            // Check write watchpoints before storing
            if (vm->enable_debugger && vm->num_watchpoints > 0) {
                debugger_check_watchpoint(vm, (void*)*VM_SP, 2, WATCH_WRITE);
            }
            VM_AX = *(short *)*VM_SP++ = VM_AX;                     // save short to address, value in ax, address on stack
        }
        else if (op == SW)    {
            // Check write watchpoints before storing
            if (vm->enable_debugger && vm->num_watchpoints > 0) {
                debugger_check_watchpoint(vm, (void*)*VM_SP, 4, WATCH_WRITE);
            }
            VM_AX = *(int *)*VM_SP++ = VM_AX;                       // save int (word) to address, value in ax, address on stack
        }
        else if (op == SI)    {
            // Check write watchpoints before storing
            if (vm->enable_debugger && vm->num_watchpoints > 0) {
                debugger_check_watchpoint(vm, (void*)*VM_SP, 8, WATCH_WRITE);
            }
            *(long long *)*VM_SP++ = VM_AX;                          // save long long to address, value in ax, address on stack
        }
        else if (op == PUSH)  {
            // DEBUG: Log pushes of pointer-like values
            if (vm->cycle <= 50 && (VM_AX > 0x100000000LL || VM_AX == 0x18)) {
                fprintf(stderr, "DEBUG PUSH cycle %lld: Pushing AX=0x%llx to SP-1=%p\n",
                        vm->cycle, VM_AX, VM_SP - 1);
            }
            *--VM_SP = VM_AX;
            // DEBUG: Verify the push
            if (vm->cycle <= 50 && (VM_AX > 0x100000000LL || VM_AX == 0x18)) {
                fprintf(stderr, "DEBUG PUSH cycle %lld: Verified *SP=0x%llx\n",
                        vm->cycle, *VM_SP);
            }
        }                                        // push the value of ax onto the stack
        else if (op == JMP)   { VM_PC = (long long *)*VM_PC; }                             // jump to the address
        else if (op == JZ)    { VM_PC = VM_AX ? VM_PC + 1 : (long long *)*VM_PC; }       // jump if ax is zero
        else if (op == JNZ)   { VM_PC = VM_AX ? (long long *)*VM_PC : VM_PC + 1; }       // jump if ax is not zero
        else if (op == CALL)  {
            // Call subroutine: push return address to main stack and shadow stack
            long long ret_addr = (long long)(VM_PC+1);
            long long call_target = *VM_PC;

            *--VM_SP = ret_addr;
            if (vm->enable_cfi) {
                *--vm->shadow_sp = ret_addr;  // Also push to shadow stack for CFI
            }
            VM_PC = (long long *)call_target;
        }
        else if (op == CALLI) {
            // Call subroutine indirect: push return address to main stack and shadow stack
            long long ret_addr = (long long)VM_PC;
            *--VM_SP = ret_addr;
            if (vm->enable_cfi) {
                *--vm->shadow_sp = ret_addr;  // Also push to shadow stack for CFI
            }
            VM_PC = (long long *)VM_AX;
        }
        else if (op == ENT)   {
            // Enter function: create new stack frame
            *--VM_SP = (long long)VM_BP;  // Save old base pointer
            VM_BP = VM_SP;                 // Set new base pointer

            // If stack canaries are enabled, write canary after old bp
            if (vm->enable_stack_canaries) {
                *--VM_SP = vm->stack_canary;
            }

            // Allocate space for local variables
            long long stack_size = *VM_PC++;
            VM_SP = VM_SP - stack_size;

            // Stack overflow checking (for stack instrumentation)
            if (vm->enable_stack_instrumentation) {
                long long stack_used = (long long)(vm->initial_sp - VM_SP);
                if (stack_used > vm->stack_high_water) {
                    vm->stack_high_water = stack_used;
                }

                // Check if we're approaching stack limit (within 10% of segment size)
                long long stack_limit = vm->poolsize * 9 / 10;  // 90% threshold
                if (stack_used > stack_limit) {
                    if (vm->stack_instr_errors) {
                        printf("\n========== STACK OVERFLOW WARNING ==========\n");
                        printf("Stack usage: %lld bytes (limit: %lld bytes)\n", stack_used, stack_limit);
                        printf("Current PC: 0x%llx (offset: %lld)\n",
                               (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
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
        }
        else if (op == ADJ)  { VM_SP = VM_SP + *VM_PC++; }                               // add esp, <size>
        else if (op == LEV)  {
            // Leave function: restore stack frame and return
            VM_SP = VM_BP;  // Reset stack pointer to base pointer

            // If stack canaries are enabled, check canary (it's at bp-1)
            if (vm->enable_stack_canaries) {
                // Canary is one slot below the saved bp
                long long canary = VM_SP[-1];
                if (canary != vm->stack_canary) {
                    printf("\n========== STACK OVERFLOW DETECTED ==========\n");
                    printf("Stack canary corrupted!\n");
                    printf("Expected: 0x%llx\n", vm->stack_canary);
                    printf("Found:    0x%llx\n", canary);
                    printf("PC:       0x%llx (offset: %lld)\n",
                           (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                    printf("A stack buffer overflow has corrupted the stack frame.\n");
                    printf("============================================\n");
                    return -1;  // Abort execution
                }
            }

            // Invalidate stack pointers for this frame (dangling pointer detection)
            if (vm->enable_dangling_detection && vm->stack_ptrs.buckets) {
                // Iterate through all stack pointers and invalidate those with matching BP
                long long current_bp = (long long)VM_BP;
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

            VM_BP = (long long *)*VM_SP++;  // Restore old base pointer

            // Get return address from main stack
            VM_PC = (long long *)*VM_SP++;  // Restore program counter (return address)

            // Check if we've returned from main (pc is NULL sentinel)
            // Skip CFI check for main's return since it was never called
            if (VM_PC == 0 || VM_PC == NULL) {
                // Main has returned, exit with return value in ax
                int ret_val = (int)VM_AX;

                fprintf(stderr, "DEBUG LEV: NULL return PC, exiting vm_eval with ret_val=%d, AX=%lld, thread=%p\n",
                        ret_val, VM_AX, thread);

                // Report memory leaks if leak detection enabled
                report_memory_leaks(vm);

                VM_RETURN(ret_val);
            }

            // CFI check: validate return address against shadow stack
            // Only for normal function returns (not main's exit)
            if (vm->enable_cfi) {
                long long shadow_ret_addr = *vm->shadow_sp++;  // Pop return address from shadow stack
                long long main_ret_addr = (long long)VM_PC;   // Return address we just loaded

                if (main_ret_addr != shadow_ret_addr) {
                    printf("\n========== CFI VIOLATION ==========\n");
                    printf("Control flow integrity violation detected!\n");
                    printf("Expected return address: 0x%llx\n", shadow_ret_addr);
                    printf("Actual return address:   0x%llx\n", main_ret_addr);
                    printf("Current PC offset:       %lld\n",
                           (long long)(VM_PC - vm->text_seg));
                    printf("This indicates a ROP attack or stack corruption.\n");
                    printf("====================================\n");
                    return -1;  // Abort execution
                }
            }
        }
        else if (op == LEA)  {
            // Load effective address (bp + offset)
            long long offset = *VM_PC++;

            // If stack canaries are enabled and this is a local variable (negative offset),
            // we need to adjust by -STACK_CANARY_SLOTS because the canary occupies bp-1
            if (vm->enable_stack_canaries && offset < 0) {
                offset -= STACK_CANARY_SLOTS;  // Shift locals down by canary slots
            }

            VM_AX = (long long)(VM_BP + offset);
        }

        else if (op == OR)  VM_AX = *VM_SP++ | VM_AX;
        else if (op == XOR) VM_AX = *VM_SP++ ^ VM_AX;
        else if (op == AND) VM_AX = *VM_SP++ & VM_AX;
        else if (op == EQ)  VM_AX = *VM_SP++ == VM_AX;
        else if (op == NE)  VM_AX = *VM_SP++ != VM_AX;
        else if (op == LT)  VM_AX = *VM_SP++ < VM_AX;
        else if (op == LE)  VM_AX = *VM_SP++ <= VM_AX;
        else if (op == GT)  VM_AX = *VM_SP++ >  VM_AX;
        else if (op == GE)  VM_AX = *VM_SP++ >= VM_AX;
        else if (op == SHL) VM_AX = *VM_SP++ << VM_AX;
        else if (op == SHR) VM_AX = *VM_SP++ >> VM_AX;
        else if (op == ADD) VM_AX = *VM_SP++ + VM_AX;
        else if (op == SUB) VM_AX = *VM_SP++ - VM_AX;
        else if (op == MUL) VM_AX = *VM_SP++ * VM_AX;
        else if (op == DIV) VM_AX = *VM_SP++ / VM_AX;
        else if (op == MOD) VM_AX = *VM_SP++ % VM_AX;

        // Checked arithmetic operations (overflow detection)
        else if (op == ADDC) {
            long long b = VM_AX;
            long long a = *VM_SP++;

            // Check for signed addition overflow
            if ((b > 0 && a > LLONG_MAX - b) ||
                (b < 0 && a < LLONG_MIN - b)) {
                printf("\n========== INTEGER OVERFLOW ==========\n");
                printf("Addition overflow detected\n");
                printf("Operands: %lld + %lld\n", a, b);
                printf("PC:       0x%llx (offset: %lld)\n",
                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                printf("======================================\n");
                return -1;
            }

            VM_AX = a + b;
        }

        else if (op == SUBC) {
            long long b = VM_AX;
            long long a = *VM_SP++;

            // Check for signed subtraction overflow
            if ((b < 0 && a > LLONG_MAX + b) ||
                (b > 0 && a < LLONG_MIN + b)) {
                printf("\n========== INTEGER OVERFLOW ==========\n");
                printf("Subtraction overflow detected\n");
                printf("Operands: %lld - %lld\n", a, b);
                printf("PC:       0x%llx (offset: %lld)\n",
                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                printf("======================================\n");
                return -1;
            }

            VM_AX = a - b;
        }

        else if (op == MULC) {
            long long b = VM_AX;
            long long a = *VM_SP++;

            // Check for signed multiplication overflow
            // Special cases first
            if (a == 0 || b == 0) {
                VM_AX = 0;
            } else {
                // Check if result would overflow
                // For signed multiplication: if a * b / a != b, overflow occurred
                if (a == LLONG_MIN || b == LLONG_MIN) {
                    // Special case: LLONG_MIN * anything except 0, 1, -1 overflows
                    if ((a == LLONG_MIN && (b != 0 && b != 1 && b != -1)) ||
                        (b == LLONG_MIN && (a != 0 && a != 1 && a != -1))) {
                        printf("\n========== INTEGER OVERFLOW ==========\n");
                        printf("Multiplication overflow detected\n");
                        printf("Operands: %lld * %lld\n", a, b);
                        printf("PC:       0x%llx (offset: %lld)\n",
                               (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                        printf("======================================\n");
                        return -1;
                    }
                } else {
                    // General overflow check using division
                    long long result = a * b;
                    if (result / a != b) {
                        printf("\n========== INTEGER OVERFLOW ==========\n");
                        printf("Multiplication overflow detected\n");
                        printf("Operands: %lld * %lld\n", a, b);
                        printf("PC:       0x%llx (offset: %lld)\n",
                               (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                        printf("======================================\n");
                        return -1;
                    }
                    VM_AX = result;
                    goto mulc_done;
                }

                VM_AX = a * b;
            }
            mulc_done:;
        }

        else if (op == DIVC) {
            long long b = VM_AX;
            long long a = *VM_SP++;

            // Check for division by zero
            if (b == 0) {
                printf("\n========== DIVISION BY ZERO ==========\n");
                printf("Attempted division by zero\n");
                printf("Operands: %lld / 0\n", a);
                printf("PC:       0x%llx (offset: %lld)\n",
                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                printf("======================================\n");
                return -1;
            }

            // Check for signed division overflow (LLONG_MIN / -1)
            if (a == LLONG_MIN && b == -1) {
                printf("\n========== INTEGER OVERFLOW ==========\n");
                printf("Division overflow detected\n");
                printf("Operands: %lld / %lld\n", a, b);
                printf("Result would overflow (LLONG_MIN / -1 = LLONG_MAX + 1)\n");
                printf("PC:       0x%llx (offset: %lld)\n",
                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                printf("======================================\n");
                return -1;
            }

            VM_AX = a / b;
        }

        // VM memory operations (self-contained, no system calls)
        else if (op == MALC) {
            // malloc: pop size from stack, allocate from heap, return pointer in ax
            long long requested_size = *VM_SP++;
            if (requested_size <= 0) {
                VM_AX = 0;  // Return NULL for invalid size
            } else {
                // Align to 8-byte boundary
                size_t size = (requested_size + 7) & ~7;

                // If heap canaries enabled, add space for rear canary (front canary is in AllocHeader)
                size_t canary_overhead = vm->enable_heap_canaries ? sizeof(long long) : 0;
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
                    header->alloc_pc = vm->text_seg ? (long long)(VM_PC - vm->text_seg) : 0;
                    header->type_kind = TY_VOID;  // Default to void* (generic pointer)

                    // If heap canaries enabled, write canaries
                    if (vm->enable_heap_canaries) {
                        header->canary = HEAP_CANARY;
                        // Write rear canary after user data
                        long long *rear_canary = (long long *)((char *)(header + 1) + header->size);
                        *rear_canary = HEAP_CANARY;
                    }

                    VM_AX = (long long)(header + 1);  // Return pointer after header

                    // Add to alloc_map for fast pointer validation
                    hashmap_put_int(&vm->alloc_map, VM_AX, header);

                    // If memory poisoning enabled, fill with 0xCD pattern
                    if (vm->enable_memory_poisoning) {
                        memset((void *)VM_AX, 0xCD, header->size);
                    }

                    // If memory tagging enabled, record pointer creation generation
                    if (vm->enable_memory_tagging) {
                        // Store pointer -> generation mapping
                        hashmap_put_int(&vm->ptr_tags, VM_AX, (void *)(long long)header->generation);

                        if (vm->debug_vm) {
                            printf("MALC: tagged reused pointer 0x%llx with generation %d\n",
                                   VM_AX, header->generation);
                        }
                    }

                    if (vm->debug_vm) {
                        printf("MALC: reused %zu bytes at 0x%llx (segregated list, block size: %zu, class: %d)\n",
                               size, VM_AX, block->size, size_class);
                    }
                    goto malc_done;
                }

                // No suitable free block - allocate from bump pointer

                // Check for overflow/OOM (safer than pointer arithmetic which can have UB)
                size_t available = (size_t)(vm->heap_end - vm->heap_ptr);
                if (total_size > available) {
                    VM_AX = 0;  // Out of memory or would overflow
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
                    header->alloc_pc = vm->text_seg ? (long long)(VM_PC - vm->text_seg) : 0;
                    header->type_kind = TY_VOID;  // Default to void* (generic pointer)

                    // If heap canaries enabled, write canaries
                    if (vm->enable_heap_canaries) {
                        header->canary = HEAP_CANARY;
                        // Write rear canary after user data
                        long long *rear_canary = (long long *)((char *)(header + 1) + size);
                        *rear_canary = HEAP_CANARY;
                    }

                    vm->heap_ptr += total_size;
                    VM_AX = (long long)(header + 1);  // Return pointer after header

                    // Add to alloc_map for fast pointer validation
                    hashmap_put_int(&vm->alloc_map, VM_AX, header);

                    // If memory poisoning enabled, fill with 0xCD pattern
                    if (vm->enable_memory_poisoning) {
                        memset((void *)VM_AX, 0xCD, size);
                    }

                    if (vm->debug_vm) {
                        printf("MALC: allocated %zu bytes at 0x%llx (bump allocator, total: %zu)\n",
                               size, VM_AX, total_size);
                    }
                }

                malc_done:
                // If leak detection enabled, track this allocation
                if (vm->enable_memory_leak_detection && VM_AX != 0) {
                    // Allocate a record (using real malloc, not VM heap!)
                    AllocRecord *record = (AllocRecord *)malloc(sizeof(AllocRecord));
                    if (record) {
                        record->address = (void *)VM_AX;
                        record->size = ((AllocHeader *)((char *)VM_AX - sizeof(AllocHeader)))->size;
                        record->alloc_pc = (long long)(VM_PC - vm->text_seg);
                        record->next = vm->alloc_list;
                        vm->alloc_list = record;
                    }
                }

                // If provenance tracking enabled, mark heap allocation
                if (vm->enable_provenance_tracking && VM_AX != 0) {
                    AllocHeader *header = (AllocHeader *)((char *)VM_AX - sizeof(AllocHeader));

                    // Create ProvenanceInfo for heap allocation
                    ProvenanceInfo *info = malloc(sizeof(ProvenanceInfo));
                    if (info) {
                        info->origin_type = 0;  // 0 = HEAP
                        info->base = VM_AX;
                        info->size = header->requested_size;  // Use requested size, not rounded

                        // Store in HashMap
                        // Use integer key API to avoid snprintf/strdup overhead
                        hashmap_put_int(&vm->provenance, VM_AX, info);
                    }
                }

                // If memory tagging enabled, record pointer creation generation
                if (vm->enable_memory_tagging && VM_AX != 0) {
                    AllocHeader *header = (AllocHeader *)((char *)VM_AX - sizeof(AllocHeader));

                    // Store pointer -> generation mapping
                    // NOTE: We store generation+1 because generation 0 would be (void*)0 (NULL),
                    // which the HashMap can't distinguish from "key not found"
                    // Cast generation to void* (HashMap stores void* values)
                    hashmap_put_int(&vm->ptr_tags, VM_AX, (void *)(long long)(header->generation + 1));

                    if (vm->debug_vm) {
                        printf("MALC: tagged pointer 0x%llx with generation %d\n",
                               VM_AX, header->generation);
                    }
                }
            }
        }
        else if (op == MFRE) {
            // free: pop pointer from stack, validate, and add to free list
            long long ptr = *VM_SP++;

            if (ptr == 0) {
                // Freeing NULL is a no-op (standard behavior)
                VM_AX = 0;
                if (vm->debug_vm) {
                    printf("MFRE: freed NULL pointer (no-op)\n");
                }
            } else {
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
                    VM_AX = 0;
                    return -1;  // Abort execution
                }

                // If heap canaries enabled, check both canaries
                if (vm->enable_heap_canaries) {
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
                if (vm->enable_memory_leak_detection) {
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
                if (vm->enable_memory_poisoning) {
                    memset((void *)ptr, 0xDD, size);
                }

                // ALWAYS mark as freed and increment generation (for double-free detection)
                header->freed = 1;
                header->generation++;

                // If UAF detection or temporal tagging enabled, quarantine the memory (don't reuse)
                // Temporal tagging requires quarantine to prevent address reuse, which would
                // make it impossible to distinguish stale pointers from valid ones
                int quarantine = vm->enable_uaf_detection || vm->enable_memory_tagging;

                if (quarantine) {
                    // Keep memory quarantined:
                    // - Do NOT remove from alloc_map (needed for CHKP validation)
                    // - Do NOT remove ptr_tags (needed for generation comparison)
                    // - Do NOT add to free list (prevents reuse)
                    if (vm->debug_vm) {
                        const char *reason = vm->enable_uaf_detection ? "UAF detection" : "memory tagging";
                        printf("MFRE: quarantined %zu bytes at 0x%llx (%s active, gen=%d)\n",
                               size, ptr, reason, header->generation);
                    }
                } else {
                    // Normal free: remove tracking and allow reuse
                    // Remove from alloc_map (no longer tracking this allocation)
                    hashmap_delete_int(&vm->alloc_map, ptr);

                    // Remove from ptr_tags if present
                    if (vm->ptr_tags.capacity > 0) {
                        hashmap_delete_int(&vm->ptr_tags, ptr);
                    }
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

                VM_AX = 0;
            }
        }
        else if (op == MCPY) {
            // memcpy: pop size, src, dest from stack, copy bytes, return dest in ax
            long long size = *VM_SP++;
            char *src = (char *)*VM_SP++;
            char *dest = (char *)*VM_SP++;
            
            // Simple byte-by-byte copy (no overlapping check)
            for (long long i = 0; i < size; i++) {
                dest[i] = src[i];
            }
            
            VM_AX = (long long)dest;  // Return destination pointer
            if (vm->debug_vm) {
                printf("MCPY: copied %lld bytes from 0x%llx to 0x%llx\n", 
                       size, (long long)src, (long long)dest);
            }
        }

        // Type conversion: sign extension (mask to size, then sign extend)
        else if (op == SX1) { VM_AX = (long long)(signed char)(VM_AX & 0xFF); }      // sign extend byte to long long
        else if (op == SX2) { VM_AX = (long long)(short)(VM_AX & 0xFFFF); }          // sign extend short to long long
        else if (op == SX4) { VM_AX = (long long)(int)(VM_AX & 0xFFFFFFFF); }        // sign extend int to long long

        // Type conversion: zero extension (mask to size)
        else if (op == ZX1) { VM_AX = VM_AX & 0xFF; }                                // zero extend byte to long long
        else if (op == ZX2) { VM_AX = VM_AX & 0xFFFF; }                              // zero extend short to long long
        else if (op == ZX4) { VM_AX = VM_AX & 0xFFFFFFFF; }                          // zero extend int to long long

        else if (op == FLD)   { VM_FAX = *(double *)VM_AX; }                          // load double from address in ax to fax
        else if (op == FST)   { *(double *)*VM_SP++ = VM_FAX; }                       // store fax to address on stack
        else if (op == FPUSH) { *--VM_SP = *(long long *)&VM_FAX; }                   // push fax onto stack (as bit pattern)
        else if (op == FADD)  { VM_FAX = *(double *)VM_SP++ + VM_FAX; }              // fax = stack + fax
        else if (op == FSUB)  { VM_FAX = *(double *)VM_SP++ - VM_FAX; }              // fax = stack - fax
        else if (op == FMUL)  { VM_FAX = *(double *)VM_SP++ * VM_FAX; }              // fax = stack * fax
        else if (op == FDIV)  { VM_FAX = *(double *)VM_SP++ / VM_FAX; }              // fax = stack / fax
        else if (op == FNEG)  { VM_FAX = -VM_FAX; }                                   // fax = -fax
        else if (op == FEQ)   { VM_AX = *(double *)VM_SP++ == VM_FAX; }              // ax = (stack == fax)
        else if (op == FNE)   { VM_AX = *(double *)VM_SP++ != VM_FAX; }              // ax = (stack != fax)
        else if (op == FLT)   { VM_AX = *(double *)VM_SP++ < VM_FAX; }               // ax = (stack < fax)
        else if (op == FLE)   { VM_AX = *(double *)VM_SP++ <= VM_FAX; }              // ax = (stack <= fax)
        else if (op == FGT)   { VM_AX = *(double *)VM_SP++ > VM_FAX; }               // ax = (stack > fax)
        else if (op == FGE)   { VM_AX = *(double *)VM_SP++ >= VM_FAX; }              // ax = (stack >= fax)
        else if (op == I2F)   { VM_FAX = (double)VM_AX; }                             // convert ax to fax
        else if (op == F2I)   { VM_AX = (long long)VM_FAX; }                          // convert fax to ax

        else if (op == CHKP) {
            // Check pointer validity (for UAF detection, bounds checking, memory tagging)
            // ax contains the pointer to check
            long long ptr = VM_AX;

            if (!vm->enable_uaf_detection && !vm->enable_bounds_checks && !vm->enable_dangling_detection && !vm->enable_memory_tagging) {
                // No pointer checking enabled, skip
                goto chkp_done;
            }

            if (ptr == 0) {
                // NULL pointer is always invalid for dereferencing
                printf("\n========== NULL POINTER DEREFERENCE ==========\n");
                printf("Attempted to dereference NULL pointer\n");
                printf("PC: 0x%llx (offset: %lld)\n",
                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                printf("============================================\n");
                return -1;
            }

            // Check for dangling stack pointer
            if (vm->enable_dangling_detection) {
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
                                   (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                            printf("==========================================\n");
                            return -1;
                        }
                    }
                }
            }

            // Check temporal memory tagging (generation mismatch)
            if (vm->enable_memory_tagging && ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
                // Check if this memory is currently allocated
                void *header_val = hashmap_get_int(&vm->alloc_map, ptr);

                if (header_val) {
                    AllocHeader *header = (AllocHeader *)header_val;

                    // Validate header magic
                    if (header && header->magic == 0xDEADBEEF) {
                        // Look up the pointer's creation generation tag
                        void *tag_val = hashmap_get_int(&vm->ptr_tags, ptr);

                        if (tag_val) {
                            // Pointer has a tag - check if it matches current generation
                            // NOTE: We stored generation+1, so subtract 1 to get actual generation
                            int creation_generation = (int)(long long)tag_val - 1;

                            if (creation_generation != header->generation) {
                                printf("\n========== TEMPORAL SAFETY VIOLATION ==========\n");
                                printf("Pointer references memory from a different allocation generation\n");
                                printf("Address:            0x%llx\n", ptr);
                                printf("Pointer tag:        %d (creation generation)\n", creation_generation);
                                printf("Current generation: %d (memory was freed and reallocated)\n", header->generation);
                                printf("Size:               %zu bytes\n", header->size);
                                printf("Allocated at PC offset: %lld\n", header->alloc_pc);
                                printf("Current PC:         0x%llx (offset: %lld)\n",
                                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                                printf("This indicates use-after-free where memory was freed and reallocated\n");
                                printf("================================================\n");
                                return -1;
                            }

                            if (vm->debug_vm) {
                                printf("CHKP: temporal tag valid - ptr 0x%llx, generation %d matches\n",
                                       ptr, creation_generation);
                            }
                        }
                        // Note: If no tag exists, it could be a derived pointer (offset from malloc'd address)
                        // We don't flag this as an error since legitimate pointer arithmetic is common
                    }
                }
            }

            // Check if pointer is in heap range
            if (ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
                // Find the allocation that contains this pointer using hash map
                // We iterate through alloc_map (O(m) where m = number of allocations)
                // instead of scanning the entire heap (O(n) where n = heap bytes)
                AllocHeader *found_header = NULL;
                long long base_addr = 0;

                // Iterate through all allocations in hash map
                for (int i = 0; i < vm->alloc_map.capacity; i++) {
                    HashEntry *entry = &vm->alloc_map.buckets[i];

                    // Skip empty or deleted entries
                    if (!entry->key || entry->key == (void *)-1)
                        continue;

                    // Integer keys have keylen == -1
                    if (entry->keylen != -1)
                        continue;

                    // Get base address and header
                    long long alloc_start = (long long)entry->key;
                    AllocHeader *header = (AllocHeader *)entry->val;

                    if (!header || header->magic != 0xDEADBEEF)
                        continue;

                    long long alloc_end = alloc_start + header->size;

                    // Allow pointer up to (and including) one past the end
                    // This is valid in C, but dereferencing it is not
                    if (ptr >= alloc_start && ptr <= alloc_end) {
                        found_header = header;
                        base_addr = alloc_start;
                        break;
                    }
                }

                if (found_header) {
                    // Check if freed (UAF detection)
                    if (vm->enable_uaf_detection && found_header->freed) {
                        printf("\n========== USE-AFTER-FREE DETECTED ==========\n");
                        printf("Attempted to access freed memory\n");
                        printf("Address:     0x%llx\n", ptr);
                        printf("Base:        0x%llx\n", base_addr);
                        printf("Offset:      %lld bytes\n", ptr - base_addr);
                        printf("Size:        %zu bytes\n", found_header->size);
                        printf("Allocated at PC offset: %lld\n", found_header->alloc_pc);
                        printf("Generation:  %d (freed)\n", found_header->generation);
                        printf("Current PC:  0x%llx (offset: %lld)\n",
                               (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                        printf("============================================\n");
                        return -1;
                    }

                    // Check bounds (use requested_size, not rounded size)
                    if (vm->enable_bounds_checks) {
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
                                   (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                            printf("=========================================\n");
                            return -1;
                        }
                    }
                }
            }

            // Pointer is valid, continue execution
            // ax is unchanged (still contains the pointer)
            chkp_done:;
        }

        else if (op == CHKB) {
            // Check array bounds
            // Stack: [base_ptr, index]
            // Operands: element_size (in *pc)
            if (!vm->enable_bounds_checks) {
                // Bounds checking not enabled, skip
                (void)*VM_PC++;  // Consume operand but don't use it
                goto chkb_done;
            }

            long long element_size = *VM_PC++;
            long long index = *VM_SP++;       // Pop index
            long long base_ptr = *VM_SP++;    // Pop base pointer

            // Check for negative index
            if (index < 0) {
                printf("\n========== ARRAY BOUNDS ERROR ==========\n");
                printf("Negative array index: %lld\n", index);
                printf("Base address: 0x%llx\n", base_ptr);
                printf("Element size: %lld bytes\n", element_size);
                printf("PC: 0x%llx (offset: %lld)\n",
                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
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
                               (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                        printf("=========================================\n");
                        return -1;
                    }
                }
            }

            // Push values back for the actual array access
            *--VM_SP = base_ptr;
            *--VM_SP = index;

            chkb_done:;
        }

        else if (op == CALLF) {
            // Foreign function call: ax contains FFI function index
            // For variadic functions, actual arg count is on stack (pushed by codegen)
            int func_idx = VM_AX;
            if (func_idx < 0 || func_idx >= vm->ffi_count) {
                printf("error: invalid FFI function index: %d\n", func_idx);
                return -1;
            }

            ForeignFunc *ff = &vm->ffi_table[func_idx];

            // Pop actual argument count from stack (pushed by codegen for all FFI calls)
            int actual_nargs = (int)*VM_SP++;

            // DEBUG: Log pthread_create arguments
            if (strcmp(ff->name, "pthread_create") == 0) {
                fprintf(stderr, "DEBUG CALLF pthread_create: nargs=%d, SP args: [0]=0x%llx [1]=0x%llx [2]=0x%llx [3]=0x%llx\n",
                        actual_nargs, actual_nargs > 0 ? VM_SP[0] : 0, actual_nargs > 1 ? VM_SP[1] : 0,
                        actual_nargs > 2 ? VM_SP[2] : 0, actual_nargs > 3 ? VM_SP[3] : 0);
            }

            if (vm->debug_vm)
                printf("CALLF: calling %s with %d args (fixed: %d, variadic: %d)\n",
                       ff->name, actual_nargs, ff->num_fixed_args, ff->is_variadic);

            // Format string validation for printf-family functions
            if (vm->enable_format_string_checks) {
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
                    long long format_str_addr = VM_SP[format_arg_index];
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

                // All arguments are long long (for integers/pointers)
                for (int i = 0; i < actual_nargs; i++)
                    arg_types[i] = &ffi_type_sint64;

                // Prepare variadic cif with actual argument count
                ffi_status status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, ff->num_fixed_args,
                                                    actual_nargs, return_type, arg_types);
                if (status != FFI_OK) {
                    printf("error: failed to prepare variadic FFI cif (status=%d)\n", status);
                    free(arg_types);
                    return -1;
                }
            } else {
                // Non-variadic function - use cached cif or prepare once
                if (!ff->arg_types) {
                    // First call - allocate and prepare
                    ff->arg_types = malloc(actual_nargs * sizeof(ffi_type *));
                    if (!ff->arg_types) {
                        printf("error: failed to allocate arg types for FFI\n");
                        return -1;
                    }

                    // All arguments are long long (for integers/pointers)
                    for (int i = 0; i < actual_nargs; i++)
                        ff->arg_types[i] = &ffi_type_sint64;

                    ffi_status status = ffi_prep_cif(&ff->cif, FFI_DEFAULT_ABI, actual_nargs,
                                                    return_type, ff->arg_types);
                    if (status != FFI_OK) {
                        printf("error: failed to prepare FFI cif (status=%d)\n", status);
                        return -1;
                    }
                }
                // Use cached cif
                arg_types = ff->arg_types;
                memcpy(&cif, &ff->cif, sizeof(ffi_cif));
            }

            // Pop arguments from stack (they were pushed right-to-left)
            void *args[actual_nargs];
            long long arg_values[actual_nargs];

            for (int i = 0; i < actual_nargs; i++) {
                arg_values[i] = *VM_SP++;
                args[i] = &arg_values[i];
                if (vm->debug_vm)
                    printf("  arg[%d] = 0x%llx (%lld)\n", i, arg_values[i], arg_values[i]);
            }

            // Call the function using libffi
            if (ff->returns_double) {
                double result;
                ffi_call(&cif, FFI_FN(ff->func_ptr), &result, args);
                VM_FAX = result;
            } else {
                long long result;
                ffi_call(&cif, FFI_FN(ff->func_ptr), &result, args);
                VM_AX = result;
            }

            // Free temporary arg_types for variadic functions
            if (ff->is_variadic && arg_types) {
                free(arg_types);
            }

#else
            // Fallback implementation without libffi - no variadic support
            // Check for variadic functions
            if (ff->is_variadic) {
                printf("error: variadic FFI functions require libffi (build with JCC_HAS_FFI=1)\n");
                return -1;
            }

            // Arguments are on stack in reverse order (arg0 at higher address)
            // sp[0] is first arg, sp[1] is second arg, etc.
            long long args[20];  // Support up to 20 arguments
            if (actual_nargs > 20) {
                printf("error: FFI function has too many arguments: %d (max 20 without libffi)\n", actual_nargs);
                return -1;
            }

            // Pop arguments from stack (they were pushed right-to-left)
            for (int i = 0; i < actual_nargs; i++) {
                args[i] = *VM_SP++;
                if (vm->debug_vm)
                    printf("  arg[%d] = 0x%llx (%lld)\n", i, args[i], args[i]);
            }

            // NOTE: No longer need to save/restore state - VM_ macros access thread->registers directly
            // FFI calls can release GIL, but other threads will use their own thread->pc/sp/bp via VM_ macros

            // Call the native function based on argument count
            if (ff->returns_double) {
                switch (actual_nargs) {
                    case 0: VM_FAX = ((double(*)())ff->func_ptr)(); break;
                    case 1: VM_FAX = ((double(*)(long long))ff->func_ptr)(args[0]); break;
                    case 2: VM_FAX = ((double(*)(long long,long long))ff->func_ptr)(args[0],args[1]); break;
                    case 3: VM_FAX = ((double(*)(long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2]); break;
                    case 4: VM_FAX = ((double(*)(long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3]); break;
                    case 5: VM_FAX = ((double(*)(long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4]); break;
                    case 6: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5]); break;
                    case 7: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6]); break;
                    case 8: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]); break;
                    case 9: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8]); break;
                    case 10: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9]); break;
                    case 11: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10]); break;
                    case 12: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11]); break;
                    case 13: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12]); break;
                    case 14: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13]); break;
                    case 15: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14]); break;
                    case 16: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15]); break;
                    case 17: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16]); break;
                    case 18: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17]); break;
                    case 19: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17],args[18]); break;
                    case 20: VM_FAX = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17],args[18],args[19]); break;
                    default:
                        printf("error: unsupported arg count for double return: %d\n", actual_nargs);
                        return -1;
                }
            } else {
                // Integer/pointer return
                switch (actual_nargs) {
                    case 0: VM_AX = ((long long(*)())ff->func_ptr)(); break;
                    case 1: VM_AX = ((long long(*)(long long))ff->func_ptr)(args[0]); break;
                    case 2: VM_AX = ((long long(*)(long long,long long))ff->func_ptr)(args[0],args[1]); break;
                    case 3: VM_AX = ((long long(*)(long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2]); break;
                    case 4: VM_AX = ((long long(*)(long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3]); break;
                    case 5: VM_AX = ((long long(*)(long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4]); break;
                    case 6: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5]); break;
                    case 7: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6]); break;
                    case 8: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]); break;
                    case 9: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8]); break;
                    case 10: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9]); break;
                    case 11: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10]); break;
                    case 12: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11]); break;
                    case 13: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12]); break;
                    case 14: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13]); break;
                    case 15: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14]); break;
                    case 16: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15]); break;
                    case 17: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16]); break;
                    case 18: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17]); break;
                    case 19: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17],args[18]); break;
                    case 20: VM_AX = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17],args[18],args[19]); break;
                    default:
                        printf("error: unsupported arg count for int return: %d\n", actual_nargs);
                        return -1;
                }
            }
#endif  // JCC_HAS_FFI

            // NOTE: No restore needed - VM_ macros already use thread->registers directly
        }

        else if (op == CHKI) {
            // Check if variable is initialized
            // Operand: stack offset (bp-relative)
            if (!vm->enable_uninitialized_detection) {
                // Uninitialized detection not enabled, skip
                (void)*VM_PC++;  // Consume operand but don't use it
                goto chki_done;
            }

            long long offset = *VM_PC++;

            // Adjust offset for stack canaries if enabled
            if (vm->enable_stack_canaries && offset < 0) {
                offset -= STACK_CANARY_SLOTS;
            }

            // Create key for HashMap lookup (bp address + offset)
            long long addr = (long long)(VM_BP + offset);

            // Check if variable is initialized
            // Use integer key API to avoid snprintf overhead
            void *init = hashmap_get_int(&vm->init_state, addr);
            if (!init) {
                printf("\n========== UNINITIALIZED VARIABLE READ ==========\n");
                printf("Attempted to read uninitialized variable\n");
                printf("Stack offset: %lld\n", offset);
                printf("Address:      0x%llx\n", addr);
                printf("BP:           0x%llx\n", (long long)VM_BP);
                printf("PC:           0x%llx (offset: %lld)\n",
                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                printf("================================================\n");
                return -1;
            }

            chki_done:;
        }

        else if (op == MARKI) {
            // Mark variable as initialized
            // Operand: stack offset (bp-relative)
            if (!vm->enable_uninitialized_detection) {
                // Uninitialized detection not enabled, skip
                (void)*VM_PC++;  // Consume operand but don't use it
                goto marki_done;
            }

            long long offset = *VM_PC++;

            // Adjust offset for stack canaries if enabled
            if (vm->enable_stack_canaries && offset < 0) {
                offset -= STACK_CANARY_SLOTS;
            }

            // Create key for HashMap (bp address + offset)
            long long addr = (long long)(VM_BP + offset);

            // Mark as initialized (use non-NULL value)
            // Use integer key API to avoid snprintf/strdup overhead
            hashmap_put_int(&vm->init_state, addr, (void*)1);

            marki_done:;
        }

        else if (op == CHKT) {
            // Check type on pointer dereference
            // Operand: expected TypeKind
            // ax contains the pointer to check
            if (!vm->enable_type_checks) {
                // Type checking not enabled, skip
                (void)*VM_PC++;  // Consume operand but don't use it
                goto chkt_done;
            }

            int expected_type = (int)*VM_PC++;
            long long ptr = VM_AX;

            // Skip check for NULL (will be caught by CHKP if enabled)
            if (ptr == 0) {
                goto chkt_done;
            }

            // Skip check for void* (TY_VOID) and generic pointers (TY_PTR)
            // These are universal pointers in C
            if (expected_type == TY_VOID || expected_type == TY_PTR) {
                goto chkt_done;
            }

            // Only check heap allocations (we can't track stack variable types at runtime)
            if (ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
                // Find the allocation header using hash map
                // O(m) iteration where m = number of allocations (much better than O(n) heap scan)
                AllocHeader *found_header = NULL;

                // Iterate through all allocations in hash map
                for (int i = 0; i < vm->alloc_map.capacity; i++) {
                    HashEntry *entry = &vm->alloc_map.buckets[i];

                    // Skip empty or deleted entries
                    if (!entry->key || entry->key == (void *)-1)
                        continue;

                    // Integer keys have keylen == -1
                    if (entry->keylen != -1)
                        continue;

                    // Get base address and header
                    long long alloc_start = (long long)entry->key;
                    AllocHeader *header = (AllocHeader *)entry->val;

                    if (!header || header->magic != 0xDEADBEEF)
                        continue;

                    long long alloc_end = alloc_start + header->size;

                    if (ptr >= alloc_start && ptr <= alloc_end) {
                        found_header = header;
                        break;
                    }
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
                                   (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                            printf("============================================\n");
                            return -1;
                        }
                    }
                }
            }

            chkt_done:;
        }

        else if (op == MARKA) {
            // Mark address for stack pointer tracking (dangling pointer detection)
            // Operands: stack offset (bp-relative), size, and scope_id (three immediate values)
            if (!vm->enable_dangling_detection && !vm->enable_stack_instrumentation) {
                // Neither dangling nor stack instrumentation enabled, skip
                (void)*VM_PC++;  // Consume offset operand
                (void)*VM_PC++;  // Consume size operand
                (void)*VM_PC++;  // Consume scope_id operand
                goto marka_done;
            }

            long long offset = *VM_PC++;
            size_t size = (size_t)*VM_PC++;
            int scope_id = (int)*VM_PC++;

            // ax contains the pointer value (address of stack variable)
            long long ptr = VM_AX;

            if (vm->debug_vm) {
                printf("MARKA: tracking pointer 0x%llx (bp=0x%llx, offset=%lld, size=%zu, scope=%d)\n",
                       ptr, (long long)VM_BP, offset, size, scope_id);
            }

            // Create StackPtrInfo
            StackPtrInfo *info = malloc(sizeof(StackPtrInfo));
            if (!info) {
                fprintf(stderr, "MARKA: failed to allocate StackPtrInfo\n");
                goto marka_done;
            }
            info->bp = (long long)VM_BP;
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

            marka_done:;
        }

        else if (op == CHKA) {
            // Check pointer alignment
            // Operand: type size (for alignment check)
            if (!vm->enable_alignment_checks) {
                // Alignment checking not enabled, skip
                (void)*VM_PC++;  // Consume operand
                goto chka_done;
            }

            size_t type_size = (size_t)*VM_PC++;
            long long ptr = VM_AX;

            // Skip check for NULL
            if (ptr == 0) {
                goto chka_done;
            }

            // Check alignment (pointer must be multiple of type size)
            if (type_size > 1 && (ptr % type_size) != 0) {
                printf("\n========== ALIGNMENT ERROR ==========\n");
                printf("Pointer is misaligned for type\n");
                printf("Address:       0x%llx\n", ptr);
                printf("Type size:     %zu bytes\n", type_size);
                printf("Required alignment: %zu bytes\n", type_size);
                printf("Current PC:    0x%llx (offset: %lld)\n",
                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                printf("=====================================\n");
                return -1;
            }

            chka_done:;
        }

        else if (op == CHKPA) {
            // Check pointer arithmetic (invalid arithmetic detection)
            // Uses provenance tracking to validate result is within bounds
            if (!vm->enable_invalid_arithmetic || !vm->enable_provenance_tracking) {
                // Need both features enabled
                goto chkpa_done;
            }

            long long ptr = VM_AX;  // Result of pointer arithmetic

            // Skip check for NULL
            if (ptr == 0) {
                goto chkpa_done;
            }

            // Look up provenance information for this pointer
            // Use integer key API to avoid snprintf overhead
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
                           (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                    printf("===============================================\n");
                    return -1;
                }
            }
            // If no provenance info, we can't validate (might be a computed pointer)

            chkpa_done:;
        }

        else if (op == MARKP) {
            // Mark provenance for pointer tracking
            // Operands: origin_type, base, size (encoded as three immediate values)
            if (!vm->enable_provenance_tracking) {
                // Provenance tracking not enabled, skip
                (void)*VM_PC++;  // Consume origin_type operand
                (void)*VM_PC++;  // Consume base operand
                (void)*VM_PC++;  // Consume size operand
                goto markp_done;
            }

            int origin_type = (int)*VM_PC++;  // 0=HEAP, 1=STACK, 2=GLOBAL
            long long base = *VM_PC++;
            size_t size = (size_t)*VM_PC++;

            // ax contains the pointer value
            long long ptr = VM_AX;

            // Create ProvenanceInfo
            ProvenanceInfo *info = malloc(sizeof(ProvenanceInfo));
            info->origin_type = origin_type;
            info->base = base;
            info->size = size;

            // Store in HashMap
            // Use integer key API to avoid snprintf/strdup overhead
            hashmap_put_int(&vm->provenance, ptr, info);

            markp_done:;
        }

        // ========== STACK INSTRUMENTATION INSTRUCTIONS ==========

        else if (op == SCOPEIN) {
            // Mark scope entry - activate all variables in this scope
            // Operand: scope_id
            if (!vm->enable_stack_instrumentation) {
                (void)*VM_PC++;  // Consume scope_id operand
                goto scopein_done;
            }

            int scope_id = (int)*VM_PC++;

            if (vm->debug_vm) {
                printf("SCOPEIN: entering scope %d (bp=0x%llx)\n", scope_id, (long long)VM_BP);
            }

            // Iterate through all metadata entries and activate those matching this scope
            for (int i = 0; i < vm->stack_var_meta.capacity; i++) {
                if (vm->stack_var_meta.buckets[i].key != NULL) {
                    StackVarMeta *meta = (StackVarMeta *)vm->stack_var_meta.buckets[i].val;
                    if (meta && meta->scope_id == scope_id) {
                        meta->is_alive = 1;
                        meta->bp = (long long)VM_BP;
                        if (vm->debug_vm) {
                            printf("  Activated variable '%s' at bp%+lld\n", meta->name, meta->offset);
                        }
                    }
                }
            }

            scopein_done:;
        }

        else if (op == SCOPEOUT) {
            // Mark scope exit - deactivate variables and detect dangling pointers
            // Operand: scope_id
            if (!vm->enable_stack_instrumentation) {
                (void)*VM_PC++;  // Consume scope_id operand
                goto scopeout_done;
            }

            int scope_id = (int)*VM_PC++;

            if (vm->debug_vm) {
                printf("SCOPEOUT: exiting scope %d (bp=0x%llx)\n", scope_id, (long long)VM_BP);
            }

            // Iterate through all metadata entries and deactivate those matching this scope
            for (int i = 0; i < vm->stack_var_meta.capacity; i++) {
                if (vm->stack_var_meta.buckets[i].key != NULL) {
                    StackVarMeta *meta = (StackVarMeta *)vm->stack_var_meta.buckets[i].val;
                    if (meta && meta->scope_id == scope_id && meta->bp == (long long)VM_BP) {
                        meta->is_alive = 0;
                        if (vm->debug_vm) {
                            printf("  Deactivated variable '%s' at bp%+lld (reads=%lld, writes=%lld)\n",
                                   meta->name, meta->offset, meta->read_count, meta->write_count);
                        }
                    }
                }
            }

            // Check for dangling pointers (pointers to variables in this scope)
            if (vm->enable_dangling_detection) {
                for (int i = 0; i < vm->stack_ptrs.capacity; i++) {
                    if (vm->stack_ptrs.buckets[i].key != NULL) {
                        StackPtrInfo *ptr_info = (StackPtrInfo *)vm->stack_ptrs.buckets[i].val;
                        if (ptr_info && ptr_info->scope_id == scope_id && ptr_info->bp == (long long)VM_BP) {
                            if (vm->stack_instr_errors) {
                                printf("\n========== DANGLING POINTER DETECTED ==========\n");
                                printf("Pointer to stack variable in scope %d still exists\n", scope_id);
                                printf("Pointer: 0x%s (bp=%+lld)\n", vm->stack_ptrs.buckets[i].key, ptr_info->offset);
                                printf("Scope is now exiting - this pointer will dangle\n");
                                printf("Current PC: 0x%llx (offset: %lld)\n",
                                       (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                                printf("==============================================\n");
                                return -1;
                            } else if (vm->debug_vm) {
                                printf("WARNING: Dangling pointer detected for scope %d\n", scope_id);
                            }
                        }
                    }
                }
            }

            scopeout_done:;
        }

        else if (op == CHKL) {
            // Check variable liveness before access
            // Operand: offset (bp-relative)
            if (!vm->enable_stack_instrumentation) {
                (void)*VM_PC++;  // Consume offset operand
                goto chkl_done;
            }

            long long offset = *VM_PC++;

            // Look up variable metadata
            // Use integer key API to avoid snprintf overhead
            StackVarMeta *meta = (StackVarMeta *)hashmap_get_int(&vm->stack_var_meta, offset);

            if (meta) {
                // Check if variable matches current BP (different frames shouldn't interfere)
                if (meta->bp != (long long)VM_BP && meta->bp != 0) {
                    // Different frame - this is use after function return
                    if (vm->stack_instr_errors) {
                        printf("\n========== USE AFTER RETURN DETECTED ==========\n");
                        printf("Variable '%s' at bp%+lld accessed after function return\n",
                               meta->name, meta->offset);
                        printf("Variable BP:  0x%llx\n", meta->bp);
                        printf("Current BP:   0x%llx\n", (long long)VM_BP);
                        printf("Current PC:   0x%llx (offset: %lld)\n",
                               (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                        printf("==============================================\n");
                        return -1;
                    }
                }

                // Check if variable is alive
                if (!meta->is_alive) {
                    if (vm->stack_instr_errors) {
                        printf("\n========== USE AFTER SCOPE DETECTED ==========\n");
                        printf("Variable '%s' at bp%+lld accessed after scope exit\n",
                               meta->name, meta->offset);
                        printf("Scope ID: %d\n", meta->scope_id);
                        printf("Current PC: 0x%llx (offset: %lld)\n",
                               (long long)VM_PC, (long long)(VM_PC - vm->text_seg));
                        printf("=============================================\n");
                        return -1;
                    } else if (vm->debug_vm) {
                        printf("WARNING: Variable '%s' accessed after scope exit\n", meta->name);
                    }
                }
            }

            chkl_done:;
        }

        else if (op == MARKR) {
            // Mark variable read access
            // Operand: offset (bp-relative)
            if (!vm->enable_stack_instrumentation) {
                (void)*VM_PC++;  // Consume offset operand
                goto markr_done;
            }

            long long offset = *VM_PC++;

            // Look up variable metadata
            // Use integer key API to avoid snprintf overhead
            StackVarMeta *meta = (StackVarMeta *)hashmap_get_int(&vm->stack_var_meta, offset);

            if (meta && meta->bp == (long long)VM_BP) {
                meta->read_count++;
                if (vm->debug_vm) {
                    printf("MARKR: '%s' read (count=%lld)\n", meta->name, meta->read_count);
                }
            }

            markr_done:;
        }

        else if (op == MARKW) {
            // Mark variable write access
            // Operand: offset (bp-relative)
            if (!vm->enable_stack_instrumentation) {
                (void)*VM_PC++;  // Consume offset operand
                goto markw_done;
            }

            long long offset = *VM_PC++;

            // Look up variable metadata
            // Use integer key API to avoid snprintf overhead
            StackVarMeta *meta = (StackVarMeta *)hashmap_get_int(&vm->stack_var_meta, offset);

            if (meta && meta->bp == (long long)VM_BP) {
                meta->write_count++;
                // Mark as initialized on first write
                if (!meta->initialized) {
                    meta->initialized = 1;
                }
                if (vm->debug_vm) {
                    printf("MARKW: '%s' write (count=%lld)\n", meta->name, meta->write_count);
                }
            }

            markw_done:;
        }

        // ========== END STACK INSTRUMENTATION INSTRUCTIONS ==========

        else if (op == SETJMP) {
            // setjmp: Save execution context to jmp_buf and return 0
            // The jmp_buf address is in ax (not on stack)
            long long *jmp_buf_ptr = (long long *)VM_AX;

            // Save VM state to jmp_buf
            // [0] = pc (return address - where longjmp will jump back to)
            // [1] = sp (stack pointer - current state)
            // [2] = bp (base pointer)
            // [3] = stack value at sp (for restoration)
            // [4] = reserved
            jmp_buf_ptr[0] = (long long)VM_PC;      // Save return address
            jmp_buf_ptr[1] = (long long)VM_SP;      // Save stack pointer
            jmp_buf_ptr[2] = (long long)VM_BP;      // Save base pointer
            jmp_buf_ptr[3] = *VM_SP;                 // Save value at top of stack!
            jmp_buf_ptr[4] = 0;                       // Reserved

            // setjmp returns 0 when called directly
            VM_AX = 0;
        }

        else if (op == LONGJMP) {
            // longjmp: Restore execution context from jmp_buf and return val
            // Stack top-to-bottom: [env, val] (env pushed last, so on top)
            long long *jmp_buf_ptr = (long long *)*VM_SP++;  // Pop jmp_buf pointer (env)
            int val = (int)*VM_SP++;                          // Pop return value

            // Check if jmp_buf_ptr is valid
            if (jmp_buf_ptr == NULL || jmp_buf_ptr == (void*)0) {
                return -1;
            }

            // Restore VM state from jmp_buf
            VM_PC = (long long *)jmp_buf_ptr[0];     // Restore program counter
            VM_SP = (long long *)jmp_buf_ptr[1];     // Restore stack pointer
            VM_BP = (long long *)jmp_buf_ptr[2];     // Restore base pointer

            // Restore the stack value that was saved!
            *VM_SP = jmp_buf_ptr[3];

            // Set return value (convert 0 to 1 per C standard)
            VM_AX = (val == 0) ? 1 : val;
        }

        // ========== THREADING OPCODES ==========

#ifndef JCC_NO_THREADS
        else if (op == THRCREATE) {
            // Create new VM thread
            // Stack: [arg1, arg2, ..., argN, func_addr, num_args]
            int num_args = (int)*VM_SP++;
            long long func_addr = *VM_SP++;

            // Collect arguments from stack
            long long *args = NULL;
            if (num_args > 0) {
                args = (long long *)malloc(num_args * sizeof(long long));
                for (int i = num_args - 1; i >= 0; i--) {
                    args[i] = *VM_SP++;
                }
            }

            // Create new VM thread
            VMThread *new_thread = vm_thread_allocate(vm, func_addr, args, num_args);
            if (!new_thread) {
                fprintf(stderr, "Failed to create thread\n");
                VM_AX = 0;
                if (args) free(args);
                goto thrcreate_done;
            }

            // Launch OS thread
            int result = pthread_create(&new_thread->thread_id, NULL, vm_thread_entry, new_thread);
            if (result != 0) {
                fprintf(stderr, "pthread_create failed: %d\n", result);
                vm_thread_destroy(new_thread);
                VM_AX = 0;
                if (args) free(args);
                goto thrcreate_done;
            }

            // Add to thread list
            new_thread->next = vm->threads;
            vm->threads = new_thread;

            // Return thread handle in ax
            VM_AX = (long long)new_thread;
            if (args) free(args);
            thrcreate_done:;
        }

        else if (op == THRJOIN) {
            // Wait for thread completion
            // Stack: [thread_handle]
            VMThread *join_thread = (VMThread *)*VM_SP++;

            if (!join_thread) {
                fprintf(stderr, "THRJOIN: NULL thread handle\n");
                VM_AX = -1;
                goto thrjoin_done;
            }

            // Release GIL while waiting (don't block other threads)
            if (vm->enable_gil) {
                gil_release(&vm->gil);
            }

            // Wait for thread to finish
            pthread_join(join_thread->thread_id, NULL);

            if (vm->enable_gil) {
                gil_acquire(&vm->gil);
            }

            // Return thread's exit code in ax
            VM_AX = join_thread->exit_code;

            // Remove from thread list and clean up
            VMThread **ptr = &vm->threads;
            while (*ptr) {
                if (*ptr == join_thread) {
                    *ptr = join_thread->next;
                    break;
                }
                ptr = &(*ptr)->next;
            }
            vm_thread_destroy(join_thread);
            thrjoin_done:;
        }

        else if (op == THRSELF) {
            // Return current thread handle
            VMThread *current = vm_thread_get_current(vm);
            VM_AX = (long long)current;
        }

        else if (op == THREXIT) {
            // Exit current thread
            // Stack: [exit_code]
            int exit_code = (int)*VM_SP++;

            VMThread *current = vm_thread_get_current(vm);
            if (current && current != vm->main_thread) {
                current->exit_code = exit_code;
                current->terminated = 1;

                // Release GIL (if enabled)
                if (vm->enable_gil) {
                    gil_release(&vm->gil);
                }

                // Exit thread
                pthread_exit(NULL);
            } else {
                // Main thread: just return normally
                VM_AX = exit_code;
            }
        }

        else if (op == GILREL) {
            // Explicitly release GIL (for long-running user code)
            if (vm->enable_gil) {
                gil_release(&vm->gil);
            }
        }

        else if (op == GILACQ) {
            // Re-acquire GIL after manual release
            if (vm->enable_gil) {
                gil_acquire(&vm->gil);
            }
        }
#endif // JCC_NO_THREADS

        else if (op == CAS) {
            // Compare-and-swap
            // Stack (top to bottom): [expected, addr]  ax = new_value
            long long expected = *VM_SP++;                // Pop expected (top of stack)
            long long *addr = (long long *)*VM_SP++;      // Pop addr (next on stack)
            long long new_val = VM_AX;
            long long current = *addr;

            if (current == expected) {
                *addr = new_val;
                VM_AX = 1;  // Success
            } else {
                VM_AX = 0;  // Failure
            }
        }
        else if (op == EXCH) {
            // Atomic exchange
            // Stack: [addr]  ax = new_value
            long long *addr = (long long *)*VM_SP++;
            long long new_val = VM_AX;
            long long old_val = *addr;
            *addr = new_val;
            VM_AX = old_val;  // Return old value
        }
        else if (op == TLSADDR) {
            // Load thread-local variable address
            long long tls_offset = *VM_PC++;

#ifndef JCC_NO_THREADS
            VMThread *thread = vm_thread_get_current(vm);
            if (thread && thread->tls_seg) {
                VM_AX = (long long)(thread->tls_seg + tls_offset);
            } else {
                printf("TLS access but thread has no TLS segment\n");
                return -1;
            }
#else
            // Single-threaded mode: use main thread TLS
            if (vm->main_thread && vm->main_thread->tls_seg) {
                VM_AX = (long long)(vm->main_thread->tls_seg + tls_offset);
            } else {
                printf("TLS access but main thread has no TLS segment\n");
                return -1;
            }
#endif
        }

#ifndef JCC_NO_THREADS
        // Mutex operations
        else if (op == MTXINIT) {
            // Initialize mutex: Stack = [mutex_addr]
            long long *mutex_addr = (long long *)*VM_SP++;

            // Check if already initialized
            VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex_addr);
            if (m && m->initialized) {
                VM_AX = 0;  // Already initialized (POSIX allows this)
            } else {
                // Create new VMMutex
                if (!m) {
                    m = (VMMutex *)calloc(1, sizeof(VMMutex));
                    hashmap_put(&vm->mutexes, (char *)mutex_addr, m);
                }
                pthread_mutex_init(&m->mutex, NULL);
                m->initialized = 1;
                m->owner_thread = 0;
                VM_AX = 0;  // Success
            }
        }
        else if (op == MTXLOCK) {
            // Lock mutex: Stack = [mutex_addr]
            long long *mutex_addr = (long long *)*VM_SP++;

            // Get or create mutex (lazy initialization for static initializers)
            VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex_addr);
            if (!m || !m->initialized) {
                // Lazy initialization
                if (!m) {
                    m = (VMMutex *)calloc(1, sizeof(VMMutex));
                    hashmap_put(&vm->mutexes, (char *)mutex_addr, m);
                }
                pthread_mutex_init(&m->mutex, NULL);
                m->initialized = 1;
                m->owner_thread = 0;
            }

            // Release GIL before blocking (if enabled)
            if (vm->enable_gil) gil_release(&vm->gil);

            pthread_mutex_lock(&m->mutex);

            // Reacquire GIL after unblocking
            if (vm->enable_gil) gil_acquire(&vm->gil);

            m->owner_thread = (long long)pthread_self();
            VM_AX = 0;  // Success
        }
        else if (op == MTXUNLOCK) {
            // Unlock mutex: Stack = [mutex_addr]
            long long *mutex_addr = (long long *)*VM_SP++;

            VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex_addr);
            if (!m || !m->initialized) {
                printf("pthread_mutex_unlock: mutex not initialized\n");
                VM_AX = 22;  // EINVAL
            } else {
                m->owner_thread = 0;
                pthread_mutex_unlock(&m->mutex);
                VM_AX = 0;  // Success
            }
        }
        else if (op == MTXDESTROY) {
            // Destroy mutex: Stack = [mutex_addr]
            long long *mutex_addr = (long long *)*VM_SP++;

            VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex_addr);
            if (m && m->initialized) {
                pthread_mutex_destroy(&m->mutex);
                m->initialized = 0;
                // Note: we keep the VMMutex struct in hashmap (could be reinitialized)
                VM_AX = 0;  // Success
            } else {
                VM_AX = 22;  // EINVAL
            }
        }
        else if (op == MTXTRY) {
            // Try lock mutex (non-blocking): Stack = [mutex_addr]
            long long *mutex_addr = (long long *)*VM_SP++;

            // Get or create mutex (lazy initialization)
            VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex_addr);
            if (!m || !m->initialized) {
                // Lazy initialization
                if (!m) {
                    m = (VMMutex *)calloc(1, sizeof(VMMutex));
                    hashmap_put(&vm->mutexes, (char *)mutex_addr, m);
                }
                pthread_mutex_init(&m->mutex, NULL);
                m->initialized = 1;
                m->owner_thread = 0;
            }

            int result = pthread_mutex_trylock(&m->mutex);
            if (result == 0) {
                // Acquired lock
                m->owner_thread = (long long)pthread_self();
                VM_AX = 0;  // Success
            } else {
                VM_AX = result;  // EBUSY (16) if already locked
            }
        }

        // Condition variable operations
        else if (op == CVINIT) {
            // Initialize condvar: Stack = [cond_addr]
            long long *cond_addr = (long long *)*VM_SP++;

            VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond_addr);
            if (cv && cv->initialized) {
                VM_AX = 0;  // Already initialized
            } else {
                if (!cv) {
                    cv = (VMCondVar *)calloc(1, sizeof(VMCondVar));
                    hashmap_put(&vm->condvars, (char *)cond_addr, cv);
                }
                pthread_cond_init(&cv->cond, NULL);
                cv->initialized = 1;
                VM_AX = 0;  // Success
            }
        }
        else if (op == CVWAIT) {
            // Condition variable wait: Stack = [mutex_addr, cond_addr]
            long long *cond_addr = (long long *)*VM_SP++;
            long long *mutex_addr = (long long *)*VM_SP++;

            // Get or create condvar (lazy initialization)
            VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond_addr);
            if (!cv || !cv->initialized) {
                if (!cv) {
                    cv = (VMCondVar *)calloc(1, sizeof(VMCondVar));
                    hashmap_put(&vm->condvars, (char *)cond_addr, cv);
                }
                pthread_cond_init(&cv->cond, NULL);
                cv->initialized = 1;
            }

            // Get mutex
            VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex_addr);
            if (!m || !m->initialized) {
                printf("pthread_cond_wait: mutex not initialized\n");
                VM_AX = 22;  // EINVAL
            } else {
                // Release GIL before blocking (pthread_cond_wait handles mutex atomically)
                if (vm->enable_gil) gil_release(&vm->gil);

                m->owner_thread = 0;  // Will be released by pthread_cond_wait
                pthread_cond_wait(&cv->cond, &m->mutex);
                m->owner_thread = (long long)pthread_self();  // Reacquired by pthread_cond_wait

                // Reacquire GIL after unblocking
                if (vm->enable_gil) gil_acquire(&vm->gil);

                VM_AX = 0;  // Success
            }
        }
        else if (op == CVSIGNAL) {
            // Signal one waiter: Stack = [cond_addr]
            long long *cond_addr = (long long *)*VM_SP++;

            VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond_addr);
            if (!cv || !cv->initialized) {
                // Lazy initialization (signal on uninitialized condvar is no-op in POSIX)
                if (!cv) {
                    cv = (VMCondVar *)calloc(1, sizeof(VMCondVar));
                    hashmap_put(&vm->condvars, (char *)cond_addr, cv);
                }
                pthread_cond_init(&cv->cond, NULL);
                cv->initialized = 1;
            }

            pthread_cond_signal(&cv->cond);
            VM_AX = 0;  // Success
        }
        else if (op == CVBCAST) {
            // Broadcast to all waiters: Stack = [cond_addr]
            long long *cond_addr = (long long *)*VM_SP++;

            VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond_addr);
            if (!cv || !cv->initialized) {
                // Lazy initialization
                if (!cv) {
                    cv = (VMCondVar *)calloc(1, sizeof(VMCondVar));
                    hashmap_put(&vm->condvars, (char *)cond_addr, cv);
                }
                pthread_cond_init(&cv->cond, NULL);
                cv->initialized = 1;
            }

            pthread_cond_broadcast(&cv->cond);
            VM_AX = 0;  // Success
        }
        else if (op == CVDESTROY) {
            // Destroy condvar: Stack = [cond_addr]
            long long *cond_addr = (long long *)*VM_SP++;

            VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond_addr);
            if (cv && cv->initialized) {
                pthread_cond_destroy(&cv->cond);
                cv->initialized = 0;
                VM_AX = 0;  // Success
            } else {
                VM_AX = 22;  // EINVAL
            }
        }
#endif

        else {
            printf("unknown instruction:%d\n", op);
            return -1;
        }
    }
}

#ifndef JCC_NO_THREADS
void cc_enable_threading(JCC *vm) {
    vm->enable_threading = 1;
    vm->gil_check_interval = 100;  // Default: yield every 100 instructions
}

void cc_disable_threading(JCC *vm) {
    vm->enable_threading = -1;  // -1 = explicitly disabled (vs 0 = not set)
}

void cc_enable_gil(JCC *vm) {
    vm->enable_gil = 1;
}
#else
// Stub functions when threading is disabled at compile time
void cc_enable_threading(JCC *vm) { (void)vm; }
void cc_disable_threading(JCC *vm) { (void)vm; }
void cc_enable_gil(JCC *vm) { (void)vm; }
#endif

void cc_init(JCC *vm, bool enable_debugger) {
#ifndef JCC_NO_THREADS
    // Save threading configuration (must be set before cc_init via cc_enable_threading/cc_enable_gil)
    int saved_enable_threading = vm->enable_threading;
    int saved_enable_gil = vm->enable_gil;
    int saved_gil_check_interval = vm->gil_check_interval;
#endif

    // Zero-initialize the VM struct
    memset(vm, 0, sizeof(JCC));

#ifndef JCC_NO_THREADS
    // Restore/set threading configuration (threading enabled by default, GIL disabled by default)
    // If user called cc_disable_threading before cc_init, respect that (saved_enable_threading will be 0 with a marker)
    // Otherwise, enable threading by default
    if (saved_enable_threading == -1) {
        // User explicitly disabled threading
        vm->enable_threading = 0;
    } else {
        // Enable threading by default
        vm->enable_threading = 1;
    }
    // Enable GIL by default when threading is enabled (required for pthread API)
    // GIL protects concurrent access to VM registers during state swapping
    vm->enable_gil = saved_enable_gil ? saved_enable_gil : (vm->enable_threading ? 1 : 0);
    vm->gil_check_interval = saved_gil_check_interval ? saved_gil_check_interval : 100;
#endif

    // Set defaults
    vm->poolsize = 256 * 1024;  // 256KB default
    vm->debug_vm = 0;

    // Return buffer will be allocated in data segment during codegen
    vm->return_buffer_size = 1024;
    vm->return_buffer = NULL;  // Will be set to data segment location

    init_macros(vm);

    // Initialize init_state HashMap for uninitialized variable detection
    vm->init_state.capacity = 0;  // Will be allocated on first use by hashmap_put
    vm->init_state.buckets = NULL;
    vm->init_state.used = 0;

    // Initialize stack_ptrs HashMap for dangling pointer detection
    vm->stack_ptrs.capacity = 0;
    vm->stack_ptrs.buckets = NULL;
    vm->stack_ptrs.used = 0;

    // Initialize provenance HashMap for provenance tracking
    vm->provenance.capacity = 0;
    vm->provenance.buckets = NULL;
    vm->provenance.used = 0;

    // Initialize stack_var_meta HashMap for stack instrumentation
    vm->stack_var_meta.capacity = 0;
    vm->stack_var_meta.buckets = NULL;
    vm->stack_var_meta.used = 0;

    // Initialize alloc_map HashMap for fast heap pointer validation
    vm->alloc_map.capacity = 0;
    vm->alloc_map.buckets = NULL;
    vm->alloc_map.used = 0;

    // Initialize ptr_tags HashMap for temporal memory tagging
    vm->ptr_tags.capacity = 0;
    vm->ptr_tags.buckets = NULL;
    vm->ptr_tags.used = 0;

    // Initialize CFI shadow stack (will be allocated if enable_cfi is set)
    vm->shadow_stack = NULL;
    vm->shadow_sp = NULL;

    // Initialize segregated free lists
    for (int i = 0; i < 12; i++) {  // NUM_SIZE_CLASSES
        vm->size_class_lists[i] = NULL;
    }
    vm->large_list = NULL;

    // Initialize stack instrumentation state
    vm->current_scope_id = 0;
    vm->current_function_scope_id = 0;
    vm->stack_high_water = 0;

    // Initialize stack canary (will be set to random or fixed value based on flag)
    // The flag enable_random_canaries will be set by CLI parsing in main.c
    // For now, initialize to fixed value; it will be regenerated if random canaries enabled
    vm->stack_canary = STACK_CANARY;

    // Add default system include path for <...> includes
    cc_system_include(vm, "./include");

    // If VM was built with libffi support, define JCC_HAS_FFI for user code
#ifdef JCC_HAS_FFI
    cc_define(vm, "JCC_HAS_FFI", "1");
#endif

#ifndef JCC_NO_THREADS
    // Initialize threading if enabled
    if (vm->enable_threading) {
        // Initialize GIL (always initialize structure, even if not used)
        gil_init(&vm->gil);

        // Create thread-local storage key
        pthread_key_create(&vm->thread_key, NULL);

        vm->threads = NULL;  // Empty thread list initially

        // Initialize mutex/condvar tracking hashmaps
        vm->mutexes.capacity = 0;
        vm->mutexes.buckets = NULL;
        vm->mutexes.used = 0;

        vm->condvars.capacity = 0;
        vm->condvars.buckets = NULL;
        vm->condvars.used = 0;
    } else {
        vm->threads = NULL;

        // Initialize mutex/condvar hashmaps even when threading disabled
        vm->mutexes.capacity = 0;
        vm->mutexes.buckets = NULL;
        vm->mutexes.used = 0;

        vm->condvars.capacity = 0;
        vm->condvars.buckets = NULL;
        vm->condvars.used = 0;
    }

    // Always allocate main thread structure for TLS support
    // (needed even in single-threaded mode for _Thread_local variables)
    vm->main_thread = vm_thread_allocate(vm, 0, NULL, 0);
    if (!vm->main_thread) {
        fprintf(stderr, "Failed to create main thread\n");
        exit(1);
    }

    // Note: stack_seg will be allocated later during compilation
    // We'll update main_thread's stack pointers after stack allocation

    // Set main thread as current (if threading enabled)
    if (vm->enable_threading) {
        vm_thread_set_current(vm, vm->main_thread);
    }

    // Set global VM pointer for pthread FFI wrappers
    __global_vm = vm;

    // Register pthread API functions
    register_pthread_api(vm);
#endif

    if (enable_debugger) {
        vm->enable_debugger = true;
        debugger_init(vm);
    } else
        vm->enable_debugger = false;
}

void cc_destroy(JCC *vm) {
    if (!vm)
        return;
    if (vm->text_seg)
        free(vm->text_seg);
    if (vm->data_seg)
        free(vm->data_seg);
    if (vm->stack_seg)
        free(vm->stack_seg);
    if (vm->heap_seg)
        free(vm->heap_seg);
    if (vm->shadow_stack)
        free(vm->shadow_stack);
    // return_buffer is part of data_seg, no need to free separately

    // Free init_state HashMap (string keys, no values to free)
    if (vm->init_state.buckets) {
        for (int i = 0; i < vm->init_state.capacity; i++) {
            HashEntry *entry = &vm->init_state.buckets[i];
            // Free string keys (keylen != -1 means string key, not integer key)
            if (entry->key && entry->key != (void *)-1 && entry->keylen != -1) {
                free(entry->key);
            }
        }
        free(vm->init_state.buckets);
    }

    // Free stack_ptrs HashMap (string keys + StackPtrInfo values)
    if (vm->stack_ptrs.buckets) {
        for (int i = 0; i < vm->stack_ptrs.capacity; i++) {
            HashEntry *entry = &vm->stack_ptrs.buckets[i];
            if (entry->key && entry->key != (void *)-1) {
                // Free string key
                if (entry->keylen != -1) {
                    free(entry->key);
                }
                // Free StackPtrInfo value
                if (entry->val) {
                    free(entry->val);
                }
            }
        }
        free(vm->stack_ptrs.buckets);
    }

    // Free provenance HashMap (string keys + ProvenanceInfo values)
    if (vm->provenance.buckets) {
        for (int i = 0; i < vm->provenance.capacity; i++) {
            HashEntry *entry = &vm->provenance.buckets[i];
            if (entry->key && entry->key != (void *)-1) {
                // Free string key
                if (entry->keylen != -1) {
                    free(entry->key);
                }
                // Free ProvenanceInfo value
                if (entry->val) {
                    free(entry->val);
                }
            }
        }
        free(vm->provenance.buckets);
    }

    // Free stack_var_meta HashMap (string keys + StackVarMeta values)
    if (vm->stack_var_meta.buckets) {
        for (int i = 0; i < vm->stack_var_meta.capacity; i++) {
            HashEntry *entry = &vm->stack_var_meta.buckets[i];
            if (entry->key && entry->key != (void *)-1) {
                // Free string key
                if (entry->keylen != -1) {
                    free(entry->key);
                }
                // Free StackVarMeta value
                if (entry->val) {
                    free(entry->val);
                }
            }
        }
        free(vm->stack_var_meta.buckets);
    }

    // Free alloc_map HashMap (integer keys, no values to free - headers are in heap_seg)
    if (vm->alloc_map.buckets)
        free(vm->alloc_map.buckets);

    // Free ptr_tags HashMap (integer keys, values are casted integers - no heap allocation)
    if (vm->ptr_tags.buckets)
        free(vm->ptr_tags.buckets);

    // Free FFI table
    if (vm->ffi_table) {
        for (int i = 0; i < vm->ffi_count; i++) {
            free(vm->ffi_table[i].name);
#ifdef JCC_HAS_FFI
            if (vm->ffi_table[i].arg_types)
                free(vm->ffi_table[i].arg_types);
#endif
        }
        free(vm->ffi_table);
    }
    
    // Free error message buffer if set
    if (vm->error_message) {
        free(vm->error_message);
        vm->error_message = NULL;
    }

#ifndef JCC_NO_THREADS
    // Clean up threading resources
    if (vm->enable_threading) {
        // Clean up all threads
        VMThread *thread = vm->threads;
        while (thread) {
            VMThread *next = thread->next;
            vm_thread_destroy(thread);
            thread = next;
        }

        // Clean up main thread (if it was separately allocated)
        if (vm->main_thread && vm->main_thread != vm->threads) {
            // Don't free stack_seg if it's the same as vm->stack_seg
            if (vm->main_thread->stack_seg != vm->stack_seg) {
                free(vm->main_thread->stack_seg);
            }
            vm->main_thread->stack_seg = NULL;  // Prevent double-free
            vm_thread_destroy(vm->main_thread);
        }

        // Destroy GIL
        gil_destroy(&vm->gil);

        // Delete thread-local storage key
        pthread_key_delete(vm->thread_key);

        // Clean up mutex tracking hashmap
        if (vm->mutexes.buckets) {
            for (int i = 0; i < vm->mutexes.capacity; i++) {
                HashEntry *entry = &vm->mutexes.buckets[i];
                if (entry->val) {
                    VMMutex *m = (VMMutex *)entry->val;
                    if (m->initialized) {
                        pthread_mutex_destroy(&m->mutex);
                    }
                    free(m);
                }
            }
            free(vm->mutexes.buckets);
        }

        // Clean up condvar tracking hashmap
        if (vm->condvars.buckets) {
            for (int i = 0; i < vm->condvars.capacity; i++) {
                HashEntry *entry = &vm->condvars.buckets[i];
                if (entry->val) {
                    VMCondVar *cv = (VMCondVar *)entry->val;
                    if (cv->initialized) {
                        pthread_cond_destroy(&cv->cond);
                    }
                    free(cv);
                }
            }
            free(vm->condvars.buckets);
        }
    }
#endif
}

void cc_print_stack_report(JCC *vm) {
    if (!vm || !vm->enable_stack_instrumentation) {
        printf("Stack instrumentation not enabled.\n");
        return;
    }

    printf("\n========== STACK INSTRUMENTATION REPORT ==========\n");
    printf("Stack high water mark: %lld bytes\n", vm->stack_high_water);
    printf("Total scopes created: %d\n", vm->current_scope_id);
    printf("\n");

    // Collect and display variable statistics
    printf("Variable Access Statistics:\n");
    printf("%-20s %10s %10s %10s %10s\n", "Variable", "Scope", "Reads", "Writes", "Status");
    printf("%-20s %10s %10s %10s %10s\n", "--------", "-----", "-----", "------", "------");

    for (int i = 0; i < vm->stack_var_meta.capacity; i++) {
        if (vm->stack_var_meta.buckets[i].key != NULL) {
            StackVarMeta *meta = (StackVarMeta *)vm->stack_var_meta.buckets[i].val;
            if (meta) {
                const char *status = meta->is_alive ? "alive" : "dead";
                printf("%-20s %10d %10lld %10lld %10s\n",
                       meta->name ? meta->name : "<unknown>",
                       meta->scope_id,
                       meta->read_count,
                       meta->write_count,
                       status);
            }
        }
    }

    printf("=================================================\n\n");
}

void cc_include(JCC *vm, const char *path) {
    strarray_push(&vm->include_paths, strdup(path));
}

void cc_system_include(JCC *vm, const char *path) {
    strarray_push(&vm->system_include_paths, strdup(path));
}

void cc_define(JCC *vm, char *name, char *buf) {
    define_macro(vm, name, buf);
}

void cc_undef(JCC *vm, char *name) {
    undef_macro(vm, name);
}

void cc_set_asm_callback(JCC *vm, JCCAsmCallback callback, void *user_data) {
    vm->asm_callback = callback;
    vm->asm_user_data = user_data;
}

void cc_register_cfunc(JCC *vm, const char *name, void *func_ptr, int num_args, int returns_double) {
    if (!vm)
        error("cc_register_cfunc: vm is NULL");
    if (!name || !func_ptr)
        error("cc_register_cfunc: name or func_ptr is NULL");

    // Expand capacity if needed
    if (vm->ffi_count >= vm->ffi_capacity) {
        vm->ffi_capacity = vm->ffi_capacity ? vm->ffi_capacity * 2 : 32;
        vm->ffi_table = realloc(vm->ffi_table, vm->ffi_capacity * sizeof(ForeignFunc));
        if (!vm->ffi_table)
            error("cc_register_cfunc: realloc failed");
    }

    // Add function to registry (non-variadic)
    vm->ffi_table[vm->ffi_count++] = (ForeignFunc){
        .name = strdup(name),
        .func_ptr = func_ptr,
        .num_args = num_args,
        .returns_double = returns_double,
        .is_variadic = 0,
        .num_fixed_args = num_args
#ifdef JCC_HAS_FFI
        , .arg_types = NULL
#endif
    };
}

void cc_register_variadic_cfunc(JCC *vm, const char *name, void *func_ptr, int num_fixed_args, int returns_double) {
    if (!vm)
        error("cc_register_variadic_cfunc: vm is NULL");
    if (!name || !func_ptr)
        error("cc_register_variadic_cfunc: name or func_ptr is NULL");

#ifndef JCC_HAS_FFI
    // Variadic functions require libffi
    error("cc_register_variadic_cfunc: variadic FFI functions require libffi (build with JCC_HAS_FFI=1)");
#else

    // Expand capacity if needed
    if (vm->ffi_count >= vm->ffi_capacity) {
        vm->ffi_capacity = vm->ffi_capacity ? vm->ffi_capacity * 2 : 32;
        vm->ffi_table = realloc(vm->ffi_table, vm->ffi_capacity * sizeof(ForeignFunc));
        if (!vm->ffi_table)
            error("cc_register_variadic_cfunc: realloc failed");
    }

    // Add variadic function to registry
    // Note: num_args will be updated dynamically during CALLF based on actual call
    // For now, we set it to num_fixed_args as a placeholder
    vm->ffi_table[vm->ffi_count++] = (ForeignFunc){
        .name = strdup(name),
        .func_ptr = func_ptr,
        .num_args = num_fixed_args,  // Will be updated during CALLF
        .returns_double = returns_double,
        .is_variadic = 1,
        .num_fixed_args = num_fixed_args,
        .arg_types = NULL  // Will be prepared during first CALLF
    };
#endif
}

int cc_dlsym(JCC *vm, const char *name, void *func_ptr, int num_args, int returns_double) {
    if (!vm || !name || !func_ptr)
        return -1;

    for (int i = 0; i < vm->ffi_count; i++) {
        if (strcmp(vm->ffi_table[i].name, name) == 0) {
            if (vm->ffi_table[i].num_args != num_args || vm->ffi_table[i].returns_double != returns_double) {
                fprintf(stderr, "error: FFI function '%s' signature mismatch\n", name);
                return -1;
            }
            vm->ffi_table[i].func_ptr = func_ptr;
            return 0;
        }
    }

    fprintf(stderr, "error: FFI function '%s' not found in bytecode\n", name);
    return -1;
}

int cc_dlopen(JCC *vm, const char *lib_path) {
    if (!vm)
        return -1;

#ifdef _WIN32
    HMODULE handle;
    if (lib_path) {
        // Open the specified dynamic library
        handle = LoadLibraryA(lib_path);
        if (!handle) {
            DWORD err = GetLastError();
            fprintf(stderr, "error: failed to load library %s: error code %lu\n", lib_path, err);
            return -1;
        }
    } else {
        // For searching all loaded libraries, use NULL handle in GetProcAddress
        handle = NULL;
    }
#else
    void *handle = dlopen(lib_path, RTLD_LAZY);
    if (!handle && lib_path) {
        fprintf(stderr, "error: failed to load library %s: %s\n", lib_path, dlerror());
        return -1;
    }
#endif

    int success_count = 0;
    int total_count = vm->ffi_count;

    // Try to resolve each FFI function
    for (int i = 0; i < vm->ffi_count; i++) {
        ForeignFunc *ff = &vm->ffi_table[i];

#ifdef _WIN32
        // Look up the symbol
        void *func_ptr = GetProcAddress(handle, ff->name);
        if (!func_ptr) {
            DWORD err = GetLastError();
            fprintf(stderr, "warning: failed to resolve symbol '%s': error code %lu\n", ff->name, err);
        } else {
            ff->func_ptr = func_ptr;
            success_count++;
            if (vm->debug_vm)
                printf("Resolved FFI function '%s' at %p\n", ff->name, func_ptr);
        }
#else
        // Clear any previous error
        dlerror();

        // Look up the symbol
        void *func_ptr = dlsym(handle, ff->name);
        const char *error = dlerror();

        if (error)
            fprintf(stderr, "warning: failed to resolve symbol '%s': %s\n", ff->name, error);
        else {
            ff->func_ptr = func_ptr;
            success_count++;
            if (vm->debug_vm)
                printf("Resolved FFI function '%s' at %p\n", ff->name, func_ptr);
        }
#endif
    }

    if (success_count == 0 && total_count > 0) {
        fprintf(stderr, "error: no FFI functions could be resolved\n");
#ifdef _WIN32
        if (lib_path)
            FreeLibrary(handle);
#else
        if (lib_path)
            dlclose(handle);
#endif
        return -1;
    }

    if (vm->debug_vm)
        printf("Loaded %d/%d FFI functions from %s\n", success_count, total_count, lib_path ? lib_path : "default libraries");

    // Don't close! Function pointers are still in use!
    // dlclose(handle); <- NO! BAD!
    return 0;
}

// Get the platform-specific path of the standard C library
static const char* find_libc() {
#ifdef _WIN32
    // On Windows, LoadLibrary searches system paths, so return just the name
    return "msvcrt.dll";
#else
    static char path[PATH_MAX];
    const char *libname;
    const char **search_paths;

#ifdef __APPLE__
    libname = "libSystem.dylib";
    const char *apple_paths[] = {"/usr/lib/", NULL};
    search_paths = apple_paths;
#elif defined(__linux__)
    libname = "libc.so.6";
    const char *linux_paths[] = {"/lib64/", "/lib/x86_64-linux-gnu/", "/lib/", "/usr/lib64/", "/usr/lib/", NULL};
    search_paths = linux_paths;
#elif defined(__FreeBSD__)
    libname = "libc.so.7";
    const char *freebsd_paths[] = {"/lib/", "/usr/lib/", NULL};
    search_paths = freebsd_paths;
#else
    libname = "libc.so";
    const char *default_paths[] = {"/lib/", "/usr/lib/", NULL};
    search_paths = default_paths;
#endif

    // Try to find the library in standard locations
    for (const char **p = search_paths; *p; p++) {
        if (snprintf(path, sizeof(path), "%s%s", *p, libname) < sizeof(path)) {
            if (access(path, F_OK) == 0)
                return path;
        }
    }
    // Fallback to just the library name if full path not found
    return libname;
#endif
}

int cc_load_libc(JCC *vm) {
    const char *libc_path = find_libc();
    if (vm->debug_vm)
        printf("Loading standard C library: %s\n", libc_path);
    return cc_dlopen(vm, libc_path);
}

int cc_run(JCC *vm, int argc, char **argv) {
    if (!vm || !vm->text_seg) {
        error("VM not initialized - call cc_compile first");
    }

#ifndef JCC_NO_THREADS
    // Initialize TLS variables for main thread
    // (done here after codegen, when all globals are known)
    if (vm->main_thread) {
        init_thread_tls(vm, vm->main_thread);
    }
#endif

    // Get entry point (main function) from text_seg[0]
    long long main_addr = vm->text_seg[0];
    
    // main_addr is an offset from text_seg
    vm->pc = vm->text_seg + main_addr;

    // Setup stack
    vm->sp = (long long *)((char *)vm->stack_seg + vm->poolsize * sizeof(long long));
    vm->bp = vm->sp;  // Initialize base pointer to top of stack

    // Setup shadow stack for CFI if enabled
    if (vm->enable_cfi) {
        vm->shadow_sp = (long long *)((char *)vm->shadow_stack + vm->poolsize * sizeof(long long));
    }

    // Save initial stack/base pointers for exit detection in vm_eval
    vm->initial_sp = vm->sp;
    vm->initial_bp = vm->bp;
    
    // Push a sentinel return address (0) so LEV can detect when main returns
    // Stack layout before main's ENT:
    // [argv] [argc] [ret=0] ← sp
    // ENT will push old_bp and set bp=sp
    *--vm->sp = (long long)argv;  // argv parameter (will be at bp+3 after ENT)
    *--vm->sp = argc;             // argc parameter (will be at bp+2 after ENT)
    *--vm->sp = 0;                // Return address = NULL (signals exit, will be at bp+1 after ENT)

#ifndef JCC_NO_THREADS
    // Sync main thread state with VM registers
    // CRITICAL: main_thread was allocated in cc_init with stale pc/sp/bp values
    // Now that we've set up the actual runtime state, copy it to main_thread
    if (vm->main_thread) {
        vm->main_thread->pc = vm->pc;
        vm->main_thread->sp = vm->sp;
        vm->main_thread->bp = vm->bp;
        vm->main_thread->ax = 0;
        vm->main_thread->fax = 0;
        // Main thread uses shared VM stack, not its own allocated stack
        vm->main_thread->stack_seg = vm->stack_seg;
    }
#endif

    // Acquire GIL for main thread (if enabled)
    if (vm->enable_gil) {
        gil_acquire(&vm->gil);
    }

    int result = vm->enable_debugger ? debugger_run(vm, argc, argv) : vm_eval(vm);

    fprintf(stderr, "DEBUG cc_run: vm_eval returned with result=%d\n", result);

    // Release GIL for main thread (if enabled)
    if (vm->enable_gil) {
        fprintf(stderr, "DEBUG cc_run: About to release GIL\n");
        gil_release(&vm->gil);
        fprintf(stderr, "DEBUG cc_run: GIL released\n");
    }

    fprintf(stderr, "DEBUG cc_run: Returning result=%d\n", result);
    return result;
}