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

// Stack canary constant for detecting stack overflows
#define STACK_CANARY 0xDEADBEEFCAFEBABELL

// Heap canary constant for detecting heap overflows
#define HEAP_CANARY 0xCAFEBABEDEADBEEFLL

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

int vm_eval(JCC *vm) {
    int op;

    vm->cycle = 0;
    while (1) {
        vm->cycle++;

        // Debugger hooks - check before executing instruction
        if (vm->enable_debugger) {
            // Check for breakpoints
            if (debugger_check_breakpoint(vm)) {
                printf("\nBreakpoint hit at PC %p (offset: %lld)\n",
                       (void*)vm->pc, (long long)(vm->pc - vm->text_seg));
                cc_debug_repl(vm);
            }

            if (vm->single_step)
                cc_debug_repl(vm);

            if (vm->step_over && vm->pc == vm->step_over_return_addr) {
                vm->step_over = 0;
                cc_debug_repl(vm);
            }

            if (vm->step_out && vm->bp != vm->step_out_bp) {
                vm->step_out = 0;
                cc_debug_repl(vm);
            }
        }

        op = *vm->pc++; // get next operation code
        if (vm->debug_vm) {
            printf("%lld> %.5s", vm->cycle,
                    &
                    "LEA  ,IMM  ,JMP  ,CALL ,CALI ,JZ   ,JNZ  ,ENT  ,ADJ  ,LEV  ,LI   ,LC   ,LS   ,LW   ,SI   ,SC   ,SS   ,SW   ,PUSH ,"
                    "OR   ,XOR  ,AND  ,EQ   ,NE   ,LT   ,GT   ,LE   ,GE   ,SHL  ,SHR  ,ADD  ,SUB  ,MUL  ,DIV  ,MOD  ,"
                    "MALC ,MFRE ,MCPY ,"
                    "SX1  ,SX2  ,SX4  ,ZX1  ,ZX2  ,ZX4  ,"
                    "FLD  ,FST  ,FADD ,FSUB ,FMUL ,FDIV ,FNEG ,FEQ  ,FNE  ,FLT  ,FLE  ,FGT  ,FGE  ,I2F  ,F2I  ,FPSH ,"
                    "CALLF,CHKB ,CHKP ,CHKT ,CHKI ,MARKI,SETJP,LONJP"[op * 6]);
            if (op <= ADJ || op == CHKB || op == CHKT || op == CHKI || op == MARKI)
                printf(" %lld\n", *vm->pc);
            else
                printf("\n");
        }
        
        if (op == IMM)        { vm->ax = *vm->pc++; }                                        // load immediate value to ax
        else if (op == LC)    { vm->ax = (long long)(*(signed char *)(long long)vm->ax); }   // load signed char, sign-extend to long long
        else if (op == LS)    { vm->ax = (long long)(*(short *)(long long)vm->ax); }         // load short, sign-extend to long long
        else if (op == LW)    { vm->ax = (long long)(*(int *)(long long)vm->ax); }           // load int, sign-extend to long long
        else if (op == LI)    { vm->ax = *(long long *)(long long)vm->ax; }                  // load long long to ax, address in ax
        else if (op == SC)    { vm->ax = *(char *)*vm->sp++ = vm->ax; }                      // save character to address, value in ax, address on stack
        else if (op == SS)    { vm->ax = *(short *)*vm->sp++ = vm->ax; }                     // save short to address, value in ax, address on stack
        else if (op == SW)    { vm->ax = *(int *)*vm->sp++ = vm->ax; }                       // save int (word) to address, value in ax, address on stack
        else if (op == SI)    { *(long long *)*vm->sp++ = vm->ax; }                          // save long long to address, value in ax, address on stack
        else if (op == PUSH)  { *--vm->sp = vm->ax; }                                        // push the value of ax onto the stack
        else if (op == JMP)   { vm->pc = (long long *)*vm->pc; }                             // jump to the address
        else if (op == JZ)    { vm->pc = vm->ax ? vm->pc + 1 : (long long *)*vm->pc; }       // jump if ax is zero
        else if (op == JNZ)   { vm->pc = vm->ax ? (long long *)*vm->pc : vm->pc + 1; }       // jump if ax is not zero
        else if (op == CALL)  { *--vm->sp = (long long)(vm->pc+1); vm->pc = (long long *)*vm->pc; } // call subroutine
        else if (op == CALLI) { *--vm->sp = (long long)vm->pc; vm->pc = (long long *)vm->ax; } // call subroutine indirect (address in ax)
        else if (op == ENT)   {
            // Enter function: create new stack frame
            *--vm->sp = (long long)vm->bp;  // Save old base pointer
            vm->bp = vm->sp;                 // Set new base pointer

            // If stack canaries are enabled, write canary after old bp
            if (vm->enable_stack_canaries) {
                *--vm->sp = STACK_CANARY;
            }

            // Allocate space for local variables
            vm->sp = vm->sp - *vm->pc++;

            // Note: For uninitialized detection, local variables are uninitialized by default
            // (not in init_state HashMap). Parameters are marked as initialized by codegen.
            // Different function frames have different bp values, so init_state keys naturally
            // separate between frames.
        }
        else if (op == ADJ)  { vm->sp = vm->sp + *vm->pc++; }                               // add esp, <size>
        else if (op == LEV)  {
            // Leave function: restore stack frame and return
            vm->sp = vm->bp;  // Reset stack pointer to base pointer

            // If stack canaries are enabled, check canary (it's at bp-1)
            if (vm->enable_stack_canaries) {
                // Canary is one slot below the saved bp
                long long canary = vm->sp[-1];
                if (canary != STACK_CANARY) {
                    printf("\n========== STACK OVERFLOW DETECTED ==========\n");
                    printf("Stack canary corrupted!\n");
                    printf("Expected: 0x%llx\n", STACK_CANARY);
                    printf("Found:    0x%llx\n", canary);
                    printf("PC:       0x%llx (offset: %lld)\n",
                           (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                    printf("A stack buffer overflow has corrupted the stack frame.\n");
                    printf("============================================\n");
                    return -1;  // Abort execution
                }
            }

            vm->bp = (long long *)*vm->sp++;  // Restore old base pointer
            vm->pc = (long long *)*vm->sp++;  // Restore program counter (return address)

            // Check if we've returned from main (pc is NULL sentinel)
            if (vm->pc == 0 || vm->pc == NULL) {
                // Main has returned, exit with return value in ax
                int ret_val = (int)vm->ax;

                // Report memory leaks if leak detection enabled
                report_memory_leaks(vm);

                return ret_val;
            }
        }
        else if (op == LEA)  {
            // Load effective address (bp + offset)
            long long offset = *vm->pc++;

            // If stack canaries are enabled and this is a local variable (negative offset),
            // we need to adjust by -1 because the canary occupies bp-1
            if (vm->enable_stack_canaries && offset < 0) {
                offset -= 1;  // Shift locals down by one slot
            }

            vm->ax = (long long)(vm->bp + offset);
        }

        else if (op == OR)  vm->ax = *vm->sp++ | vm->ax;
        else if (op == XOR) vm->ax = *vm->sp++ ^ vm->ax;
        else if (op == AND) vm->ax = *vm->sp++ & vm->ax;
        else if (op == EQ)  vm->ax = *vm->sp++ == vm->ax;
        else if (op == NE)  vm->ax = *vm->sp++ != vm->ax;
        else if (op == LT)  vm->ax = *vm->sp++ < vm->ax;
        else if (op == LE)  vm->ax = *vm->sp++ <= vm->ax;
        else if (op == GT)  vm->ax = *vm->sp++ >  vm->ax;
        else if (op == GE)  vm->ax = *vm->sp++ >= vm->ax;
        else if (op == SHL) vm->ax = *vm->sp++ << vm->ax;
        else if (op == SHR) vm->ax = *vm->sp++ >> vm->ax;
        else if (op == ADD) vm->ax = *vm->sp++ + vm->ax;
        else if (op == SUB) vm->ax = *vm->sp++ - vm->ax;
        else if (op == MUL) vm->ax = *vm->sp++ * vm->ax;
        else if (op == DIV) vm->ax = *vm->sp++ / vm->ax;
        else if (op == MOD) vm->ax = *vm->sp++ % vm->ax;

        // VM memory operations (self-contained, no system calls)
        else if (op == MALC) {
            // malloc: pop size from stack, allocate from heap, return pointer in ax
            long long requested_size = *vm->sp++;
            if (requested_size <= 0) {
                vm->ax = 0;  // Return NULL for invalid size
            } else {
                // Align to 8-byte boundary
                size_t size = (requested_size + 7) & ~7;

                // If heap canaries enabled, add space for rear canary (front canary is in AllocHeader)
                size_t canary_overhead = vm->enable_heap_canaries ? sizeof(long long) : 0;
                size_t total_size = size + sizeof(AllocHeader) + canary_overhead;

                // Try to find a suitable block in free list (first-fit strategy)
                FreeBlock **prev = &vm->free_list;
                FreeBlock *curr = vm->free_list;
                while (curr) {
                    if (curr->size >= size) {
                        // Found a suitable block - remove from free list
                        *prev = curr->next;

                        // Restore the header (it was overwritten by FreeBlock)
                        AllocHeader *header = (AllocHeader *)curr;
                        header->size = curr->size;  // Use the actual block size
                        header->requested_size = size;  // Store requested size for bounds checking
                        header->magic = 0xDEADBEEF;
                        header->freed = 0;
                        header->generation = 0;
                        header->alloc_pc = vm->text_seg ? (long long)(vm->pc - vm->text_seg) : 0;
                        header->type_kind = TY_VOID;  // Default to void* (generic pointer)

                        // If heap canaries enabled, write canaries
                        if (vm->enable_heap_canaries) {
                            header->canary = HEAP_CANARY;
                            // Write rear canary after user data
                            long long *rear_canary = (long long *)((char *)(header + 1) + header->size);
                            *rear_canary = HEAP_CANARY;
                        }

                        vm->ax = (long long)(header + 1);  // Return pointer after header
                        if (vm->debug_vm) {
                            printf("MALC: reused %zu bytes at 0x%llx (from free list, block size: %zu)\n",
                                   size, vm->ax, curr->size);
                        }
                        goto malc_done;
                    }
                    prev = &curr->next;
                    curr = curr->next;
                }

                // No suitable free block - allocate from bump pointer
                if (vm->heap_ptr + total_size > vm->heap_end) {
                    vm->ax = 0;  // Out of memory
                    if (vm->debug_vm) {
                        printf("MALC: out of memory (requested %zu bytes, need %zu total)\n",
                               size, total_size);
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
                    if (vm->enable_heap_canaries) {
                        header->canary = HEAP_CANARY;
                        // Write rear canary after user data
                        long long *rear_canary = (long long *)((char *)(header + 1) + size);
                        *rear_canary = HEAP_CANARY;
                    }

                    vm->heap_ptr += total_size;
                    vm->ax = (long long)(header + 1);  // Return pointer after header

                    if (vm->debug_vm) {
                        printf("MALC: allocated %zu bytes at 0x%llx (bump allocator, total: %zu)\n",
                               size, vm->ax, total_size);
                    }
                }

                malc_done:
                // If leak detection enabled, track this allocation
                if (vm->enable_memory_leak_detection && vm->ax != 0) {
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
            }
        }
        else if (op == MFRE) {
            // free: pop pointer from stack, validate, and add to free list
            long long ptr = *vm->sp++;

            if (ptr == 0) {
                // Freeing NULL is a no-op (standard behavior)
                vm->ax = 0;
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
                    vm->ax = 0;
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

                // If UAF detection enabled, mark as freed but don't reuse
                if (vm->enable_uaf_detection) {
                    header->freed = 1;
                    header->generation++;
                    if (vm->debug_vm) {
                        printf("MFRE: marked %zu bytes at 0x%llx as freed (UAF detection active, gen=%d)\n",
                               size, ptr, header->generation);
                    }
                } else {
                    // Normal free: add this block to the free list
                    // We reuse the memory for the FreeBlock structure
                    FreeBlock *block = (FreeBlock *)header;
                    block->size = size;
                    block->next = vm->free_list;
                    vm->free_list = block;

                    if (vm->debug_vm) {
                        printf("MFRE: freed %zu bytes at 0x%llx (added to free list)\n", size, ptr);
                    }
                }

                vm->ax = 0;
            }
        }
        else if (op == MCPY) {
            // memcpy: pop size, src, dest from stack, copy bytes, return dest in ax
            long long size = *vm->sp++;
            char *src = (char *)*vm->sp++;
            char *dest = (char *)*vm->sp++;
            
            // Simple byte-by-byte copy (no overlapping check)
            for (long long i = 0; i < size; i++) {
                dest[i] = src[i];
            }
            
            vm->ax = (long long)dest;  // Return destination pointer
            if (vm->debug_vm) {
                printf("MCPY: copied %lld bytes from 0x%llx to 0x%llx\n", 
                       size, (long long)src, (long long)dest);
            }
        }

        // Type conversion: sign extension (mask to size, then sign extend)
        else if (op == SX1) { vm->ax = (long long)(signed char)(vm->ax & 0xFF); }      // sign extend byte to long long
        else if (op == SX2) { vm->ax = (long long)(short)(vm->ax & 0xFFFF); }          // sign extend short to long long
        else if (op == SX4) { vm->ax = (long long)(int)(vm->ax & 0xFFFFFFFF); }        // sign extend int to long long

        // Type conversion: zero extension (mask to size)
        else if (op == ZX1) { vm->ax = vm->ax & 0xFF; }                                // zero extend byte to long long
        else if (op == ZX2) { vm->ax = vm->ax & 0xFFFF; }                              // zero extend short to long long
        else if (op == ZX4) { vm->ax = vm->ax & 0xFFFFFFFF; }                          // zero extend int to long long

        else if (op == FLD)   { vm->fax = *(double *)vm->ax; }                          // load double from address in ax to fax
        else if (op == FST)   { *(double *)*vm->sp++ = vm->fax; }                       // store fax to address on stack
        else if (op == FPUSH) { *--vm->sp = *(long long *)&vm->fax; }                   // push fax onto stack (as bit pattern)
        else if (op == FADD)  { vm->fax = *(double *)vm->sp++ + vm->fax; }              // fax = stack + fax
        else if (op == FSUB)  { vm->fax = *(double *)vm->sp++ - vm->fax; }              // fax = stack - fax
        else if (op == FMUL)  { vm->fax = *(double *)vm->sp++ * vm->fax; }              // fax = stack * fax
        else if (op == FDIV)  { vm->fax = *(double *)vm->sp++ / vm->fax; }              // fax = stack / fax
        else if (op == FNEG)  { vm->fax = -vm->fax; }                                   // fax = -fax
        else if (op == FEQ)   { vm->ax = *(double *)vm->sp++ == vm->fax; }              // ax = (stack == fax)
        else if (op == FNE)   { vm->ax = *(double *)vm->sp++ != vm->fax; }              // ax = (stack != fax)
        else if (op == FLT)   { vm->ax = *(double *)vm->sp++ < vm->fax; }               // ax = (stack < fax)
        else if (op == FLE)   { vm->ax = *(double *)vm->sp++ <= vm->fax; }              // ax = (stack <= fax)
        else if (op == FGT)   { vm->ax = *(double *)vm->sp++ > vm->fax; }               // ax = (stack > fax)
        else if (op == FGE)   { vm->ax = *(double *)vm->sp++ >= vm->fax; }              // ax = (stack >= fax)
        else if (op == I2F)   { vm->fax = (double)vm->ax; }                             // convert ax to fax
        else if (op == F2I)   { vm->ax = (long long)vm->fax; }                          // convert fax to ax

        else if (op == CHKP) {
            // Check pointer validity (for UAF detection and bounds checking)
            // ax contains the pointer to check
            long long ptr = vm->ax;

            if (!vm->enable_uaf_detection && !vm->enable_bounds_checks) {
                // Neither UAF nor bounds checking enabled, skip
                goto chkp_done;
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

            // Check if pointer is in heap range
            if (ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
                // Find the allocation that contains this pointer
                // We need to search for it since ptr might be offset from the base
                char *search_ptr = vm->heap_seg;
                AllocHeader *found_header = NULL;
                long long base_addr = 0;

                while (search_ptr < vm->heap_ptr) {
                    AllocHeader *header = (AllocHeader *)search_ptr;

                    if (header->magic != 0xDEADBEEF) {
                        // Not a valid allocation, skip
                        search_ptr += sizeof(AllocHeader) + 8;  // Minimum allocation
                        continue;
                    }

                    long long alloc_start = (long long)(header + 1);
                    long long alloc_end = alloc_start + header->size;

                    // Allow pointer up to (and including) one past the end
                    // This is valid in C, but dereferencing it is not
                    if (ptr >= alloc_start && ptr <= alloc_end) {
                        found_header = header;
                        base_addr = alloc_start;
                        break;
                    }

                    // Move to next allocation
                    size_t canary_overhead = vm->enable_heap_canaries ? sizeof(long long) : 0;
                    search_ptr += sizeof(AllocHeader) + header->size + canary_overhead;
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
                               (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
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
                                   (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
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
                (void)*vm->pc++;  // Consume operand but don't use it
                goto chkb_done;
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

            chkb_done:;
        }

        else if (op == CALLF) {
            // Foreign function call: ax contains FFI function index
            // For variadic functions, actual arg count is on stack (pushed by codegen)
            int func_idx = vm->ax;
            if (func_idx < 0 || func_idx >= vm->ffi_count) {
                printf("error: invalid FFI function index: %d\n", func_idx);
                return -1;
            }

            ForeignFunc *ff = &vm->ffi_table[func_idx];

            // Pop actual argument count from stack (pushed by codegen for all FFI calls)
            int actual_nargs = (int)*vm->sp++;

            if (vm->debug_vm)
                printf("CALLF: calling %s with %d args (fixed: %d, variadic: %d)\n",
                       ff->name, actual_nargs, ff->num_fixed_args, ff->is_variadic);

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
                args[i] = *vm->sp++;
                if (vm->debug_vm)
                    printf("  arg[%d] = 0x%llx (%lld)\n", i, args[i], args[i]);
            }

            // Call the native function based on argument count
            if (ff->returns_double) {
                switch (actual_nargs) {
                    case 0: vm->fax = ((double(*)())ff->func_ptr)(); break;
                    case 1: vm->fax = ((double(*)(long long))ff->func_ptr)(args[0]); break;
                    case 2: vm->fax = ((double(*)(long long,long long))ff->func_ptr)(args[0],args[1]); break;
                    case 3: vm->fax = ((double(*)(long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2]); break;
                    case 4: vm->fax = ((double(*)(long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3]); break;
                    case 5: vm->fax = ((double(*)(long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4]); break;
                    case 6: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5]); break;
                    case 7: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6]); break;
                    case 8: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]); break;
                    case 9: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8]); break;
                    case 10: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9]); break;
                    case 11: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10]); break;
                    case 12: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11]); break;
                    case 13: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12]); break;
                    case 14: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13]); break;
                    case 15: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14]); break;
                    case 16: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15]); break;
                    case 17: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16]); break;
                    case 18: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17]); break;
                    case 19: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17],args[18]); break;
                    case 20: vm->fax = ((double(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17],args[18],args[19]); break;
                    default:
                        printf("error: unsupported arg count for double return: %d\n", actual_nargs);
                        return -1;
                }
            } else {
                // Integer/pointer return
                switch (actual_nargs) {
                    case 0: vm->ax = ((long long(*)())ff->func_ptr)(); break;
                    case 1: vm->ax = ((long long(*)(long long))ff->func_ptr)(args[0]); break;
                    case 2: vm->ax = ((long long(*)(long long,long long))ff->func_ptr)(args[0],args[1]); break;
                    case 3: vm->ax = ((long long(*)(long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2]); break;
                    case 4: vm->ax = ((long long(*)(long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3]); break;
                    case 5: vm->ax = ((long long(*)(long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4]); break;
                    case 6: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5]); break;
                    case 7: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6]); break;
                    case 8: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]); break;
                    case 9: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8]); break;
                    case 10: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9]); break;
                    case 11: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10]); break;
                    case 12: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11]); break;
                    case 13: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12]); break;
                    case 14: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13]); break;
                    case 15: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14]); break;
                    case 16: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15]); break;
                    case 17: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16]); break;
                    case 18: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17]); break;
                    case 19: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17],args[18]); break;
                    case 20: vm->ax = ((long long(*)(long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long,long long))ff->func_ptr)(args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15],args[16],args[17],args[18],args[19]); break;
                    default:
                        printf("error: unsupported arg count for int return: %d\n", actual_nargs);
                        return -1;
                }
            }
#endif  // JCC_HAS_FFI
        }

        else if (op == CHKI) {
            // Check if variable is initialized
            // Operand: stack offset (bp-relative)
            if (!vm->enable_uninitialized_detection) {
                // Uninitialized detection not enabled, skip
                (void)*vm->pc++;  // Consume operand but don't use it
                goto chki_done;
            }

            long long offset = *vm->pc++;

            // Adjust offset for stack canaries if enabled
            if (vm->enable_stack_canaries && offset < 0) {
                offset -= 1;
            }

            // Create key for HashMap lookup (bp address + offset)
            long long addr = (long long)(vm->bp + offset);
            char key[32];
            snprintf(key, sizeof(key), "%lld", addr);

            // Check if variable is initialized
            void *init = hashmap_get(&vm->init_state, key);
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

            chki_done:;
        }

        else if (op == MARKI) {
            // Mark variable as initialized
            // Operand: stack offset (bp-relative)
            if (!vm->enable_uninitialized_detection) {
                // Uninitialized detection not enabled, skip
                (void)*vm->pc++;  // Consume operand but don't use it
                goto marki_done;
            }

            long long offset = *vm->pc++;

            // Adjust offset for stack canaries if enabled
            if (vm->enable_stack_canaries && offset < 0) {
                offset -= 1;
            }

            // Create key for HashMap (bp address + offset)
            long long addr = (long long)(vm->bp + offset);
            char key_buf[32];
            snprintf(key_buf, sizeof(key_buf), "%lld", addr);

            // strdup() the key since hashmap stores the pointer
            char *key = strdup(key_buf);

            // Mark as initialized (use non-NULL value)
            hashmap_put(&vm->init_state, key, (void*)1);

            marki_done:;
        }

        else if (op == CHKT) {
            // Check type on pointer dereference
            // Operand: expected TypeKind
            // ax contains the pointer to check
            if (!vm->enable_type_checks) {
                // Type checking not enabled, skip
                (void)*vm->pc++;  // Consume operand but don't use it
                goto chkt_done;
            }

            int expected_type = (int)*vm->pc++;
            long long ptr = vm->ax;

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
                // Find the allocation header
                char *search_ptr = vm->heap_seg;
                AllocHeader *found_header = NULL;

                while (search_ptr < vm->heap_ptr) {
                    AllocHeader *header = (AllocHeader *)search_ptr;

                    if (header->magic != 0xDEADBEEF) {
                        search_ptr += sizeof(AllocHeader) + 8;
                        continue;
                    }

                    long long alloc_start = (long long)(header + 1);
                    long long alloc_end = alloc_start + header->size;

                    if (ptr >= alloc_start && ptr <= alloc_end) {
                        found_header = header;
                        break;
                    }

                    size_t canary_overhead = vm->enable_heap_canaries ? sizeof(long long) : 0;
                    search_ptr += sizeof(AllocHeader) + header->size + canary_overhead;
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

            chkt_done:;
        }

        else if (op == SETJMP) {
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
        }

        else if (op == LONGJMP) {
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
        }

        else {
            printf("unknown instruction:%d\n", op);
            return -1;
        }
    }
}

void cc_init(JCC *vm, bool enable_debugger) {
    // Zero-initialize the VM struct
    memset(vm, 0, sizeof(JCC));

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

    // Add default system include path for <...> includes
    cc_system_include(vm, "./include");

    // If VM was built with libffi support, define JCC_HAS_FFI for user code
#ifdef JCC_HAS_FFI
    cc_define(vm, "JCC_HAS_FFI", "1");
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
    // return_buffer is part of data_seg, no need to free separately

    // Free init_state HashMap
    if (vm->init_state.buckets)
        free(vm->init_state.buckets);

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
 
    // Get entry point (main function) from text_seg[0]
    long long main_addr = vm->text_seg[0];
    
    // main_addr is an offset from text_seg
    vm->pc = vm->text_seg + main_addr;

    // Setup stack
    vm->sp = (long long *)((char *)vm->stack_seg + vm->poolsize * sizeof(long long));
    vm->bp = vm->sp;  // Initialize base pointer to top of stack
    
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

    return vm->enable_debugger ? debugger_run(vm, argc, argv) : vm_eval(vm);
}