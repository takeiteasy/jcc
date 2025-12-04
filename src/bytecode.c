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
*/

#include "jcc.h"
#include "./internal.h"

// Bytecode file format:
// V1:
//   Magic: "JCC\0" (4 bytes)
//   Version: 1 (4 bytes)
//   Text size: size in bytes (8 bytes)
//   Data size: size in bytes (8 bytes)
//   Main offset: offset of main() in text segment (8 bytes)
//   Text segment: bytecode (text_size bytes)
//   Data segment: global data (data_size bytes)
// V2 (adds flags):
//   Magic: "JCC\0" (4 bytes)
//   Version: 2 (4 bytes)
//   Flags: JCCFlags bitfield (4 bytes)
//   Text size: size in bytes (8 bytes)
//   Data size: size in bytes (8 bytes)
//   Main offset: offset of main() in text segment (8 bytes)
//   Text segment: bytecode (text_size bytes)
//   Data segment: global data (data_size bytes)

int cc_save_bytecode(JCC *vm, const char *path) {
    if (!vm || !path) {
        fprintf(stderr, "error: invalid arguments to cc_save_bytecode\n");
        return -1;
    }
    
    if (!vm->text_seg || !vm->data_seg) {
        fprintf(stderr, "error: no bytecode to save (compile first)\n");
        return -1;
    }
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "error: failed to open %s for writing: %s\n", path, strerror(errno));
        return -1;
    }
    
    // Calculate sizes
    long long text_size = (vm->text_ptr - vm->text_seg + 1) * sizeof(long long);
    long long data_size = vm->data_ptr - vm->data_seg;
    long long main_offset = vm->text_seg[0];  // main() entry point
    
    // Create a copy of text segment with absolute addresses converted to relative offsets
    long long *text_copy = malloc(text_size);
    if (!text_copy) {
        fprintf(stderr, "error: failed to allocate temporary buffer\n");
        fclose(f);
        return -1;
    }
    memcpy(text_copy, vm->text_seg, text_size);
    
    // Convert absolute addresses to relative offsets for relocatable instructions
    // Scan through bytecode and fix CALL, JMP, JZ, JNZ instructions
    long long num_instructions = text_size / sizeof(long long);
    long long text_base = (long long)vm->text_seg;
    long long text_end = text_base + text_size;
    
    // Two-pass approach to avoid operand misalignment:
    // CRITICAL: Cannot use i++ to skip operands in a for loop that also increments i
    // This causes double-skipping (operand becomes misread as opcode on next iteration)
    // Pass 1: Mark all operand positions in boolean array
    // Pass 2: Process only opcodes (skip marked operands), convert addresses to offsets
    // Mark which positions are operands (not opcodes) so we skip them
    char *is_operand = calloc(num_instructions, 1);
    for (long long i = 0; i < num_instructions; i++) {
        int op = text_copy[i];
        // Instructions with immediate operands
        if (op == IMM || op == LEA || op == ENT || op == ADJ ||
            op == CALL || op == JMP || op == JZ || op == JNZ) {
            if (i + 1 < num_instructions) {
                is_operand[i + 1] = 1;  // Mark next position as operand
            }
        }
    }
    
    // Now convert addresses to offsets
    for (long long i = 0; i < num_instructions; i++) {
        if (is_operand[i]) continue;  // Skip operands
        
        int op = text_copy[i];
        if (op == CALL || op == JMP || op == JZ || op == JNZ) {
            // Next value should be an address - convert to offset
            if (i + 1 < num_instructions) {
                long long value = text_copy[i + 1];
                // Only convert if this is definitely a pointer (large value within text segment)
                // Heuristic: values > 65536 are likely pointers, not small immediate values
                if (value > 65536 && value >= text_base && value < text_end) {
                    // Convert absolute address to relative offset (in instructions, not bytes)
                    long long offset = (value - text_base) / sizeof(long long);
                    if (vm->debug_vm) {
                        printf("Converting %s at [%lld]: addr 0x%llx -> offset %lld\n",
                               (op == CALL ? "CALL" : op == JMP ? "JMP" : op == JZ ? "JZ" : "JNZ"),
                               i, value, offset);
                    }
                    text_copy[i + 1] = offset;
                } else if (vm->debug_vm) {
                    printf("WARNING: %s at [%lld] has suspicious value %lld (0x%llx)\n",
                           (op == CALL ? "CALL" : op == JMP ? "JMP" : op == JZ ? "JZ" : "JNZ"),
                           i, value, value);
                }
            }
        }
    }
    free(is_operand);
    
    // Write header
    if (fwrite(JCC_MAGIC, 1, 4, f) != 4) {
        fprintf(stderr, "error: failed to write magic: %s\n", strerror(errno));
        fclose(f);
        return -1;
    }
    
    int version = JCC_VERSION;
    if (fwrite(&version, sizeof(int), 1, f) != 1) {
        fprintf(stderr, "error: failed to write version: %s\n", strerror(errno));
        fclose(f);
        return -1;
    }

    // Write flags (v2 format)
    uint32_t flags = vm->flags;
    if (fwrite(&flags, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "error: failed to write flags: %s\n", strerror(errno));
        fclose(f);
        return -1;
    }

    if (fwrite(&text_size, sizeof(long long), 1, f) != 1) {
        fprintf(stderr, "error: failed to write text size: %s\n", strerror(errno));
        fclose(f);
        return -1;
    }
    
    if (fwrite(&data_size, sizeof(long long), 1, f) != 1) {
        fprintf(stderr, "error: failed to write data size: %s\n", strerror(errno));
        fclose(f);
        return -1;
    }
    
    if (fwrite(&main_offset, sizeof(long long), 1, f) != 1) {
        fprintf(stderr, "error: failed to write main offset: %s\n", strerror(errno));
        fclose(f);
        return -1;
    }
    
    // Write text segment (using the copy with relative offsets)
    if (fwrite(text_copy, 1, text_size, f) != (size_t)text_size) {
        fprintf(stderr, "error: failed to write text segment: %s\n", strerror(errno));
        free(text_copy);
        fclose(f);
        return -1;
    }
    free(text_copy);
    
    // Write data segment
    if (data_size > 0) {
        if (fwrite(vm->data_seg, 1, data_size, f) != (size_t)data_size) {
            fprintf(stderr, "error: failed to write data segment: %s\n", strerror(errno));
            fclose(f);
            return -1;
        }
    }
    
    fclose(f);
    
    if (vm->debug_vm) {
        printf("Saved bytecode to %s:\n", path);
        printf("  Text size: %lld bytes\n", text_size);
        printf("  Data size: %lld bytes\n", data_size);
        printf("  Main offset: %lld\n", main_offset);
    }
    
    return 0;
}

static int load_bytecode(JCC *vm, const char *data, size_t size) {
    const char *cursor = data;
    const char *end = data + size;

#define READ_AND_INCR(VAR, TYPE)                                     \
    if (cursor + sizeof(TYPE) > end)                                 \
    {                                                                \
        fprintf(stderr, "error: unexpected end of bytecode data\n"); \
        return -1;                                                   \
    }                                                                \
    TYPE VAR = *(TYPE *)cursor;                                      \
    cursor += sizeof(TYPE);

    // Read magic
    if (memcmp(cursor, JCC_MAGIC, 4) != 0) {
        fprintf(stderr, "error: invalid bytecode file (bad magic)\n");
        return -1;
    }
    cursor += 4;
    
    // Read version
    READ_AND_INCR(version, int);
    if (version != 1 && version != 2) {
        fprintf(stderr, "error: unsupported bytecode version %d (expected 1 or 2)\n", version);
        return -1;
    }

    // Read flags (v2 only)
    if (version == 2) {
        READ_AND_INCR(flags, uint32_t);
        vm->flags = flags;
    }

    // Read sizes
    READ_AND_INCR(text_size, long long);
    READ_AND_INCR(data_size, long long);
    READ_AND_INCR(main_offset, long long);
    if (text_size < 0 || data_size < 0 || cursor + text_size + data_size > end) {
        fprintf(stderr, "error: invalid bytecode sizes\n");
        return -1;
    }
    
    // Allocate segments
    vm->text_seg = calloc(vm->poolsize, sizeof(long long));
    vm->data_seg = calloc(vm->poolsize, 1);
    vm->stack_seg = calloc(vm->poolsize, sizeof(long long));
    vm->heap_seg = calloc(vm->poolsize, 1);
    if (!vm->text_seg || !vm->data_seg || !vm->stack_seg || !vm->heap_seg) {
        fprintf(stderr, "error: failed to allocate memory segments\n");
        return -1;
    }
    
    // Read text segment
    READ_AND_INCR(text_data, size_t);
    if (text_data != (size_t)text_size) {
        fprintf(stderr, "error: text segment size mismatch\n");
        return -1;
    }
    
    // Read data segment
    if (data_size > 0) {
        READ_AND_INCR(data_data, size_t);
        if (data_data != (size_t)data_size) {
            fprintf(stderr, "error: data segment size mismatch\n");
            return -1;
        }
    }
    
    // Set up pointers
    vm->text_ptr = vm->text_seg + (text_size / sizeof(long long)) - 1;
    vm->data_ptr = vm->data_seg + data_size;
    vm->heap_ptr = vm->heap_seg;
    vm->heap_end = vm->heap_seg + vm->poolsize;
    vm->free_list = NULL;
    
    // Convert relative offsets back to absolute addresses
    // Scan through bytecode and fix CALL, JMP, JZ, JNZ instructions
    long long num_instructions = text_size / sizeof(long long);
    
    // Two-pass approach to avoid operand misalignment (inverse of compiler's save operation)
    // Pass 1: Mark all operand positions in boolean array
    // Pass 2: Process only opcodes (skip marked operands), convert offsets to addresses
    // Mark which positions are operands (not opcodes) so we skip them
    char *is_operand = calloc(num_instructions, 1);
    for (long long i = 0; i < num_instructions; i++) {
        int op = vm->text_seg[i];
        // Instructions with immediate operands
        if (op == IMM || op == LEA || op == ENT || op == ADJ ||
            op == CALL || op == JMP || op == JZ || op == JNZ) {
            if (i + 1 < num_instructions) {
                is_operand[i + 1] = 1;  // Mark next position as operand
            }
        }
    }
    
    // Now convert offsets to addresses
    for (long long i = 0; i < num_instructions; i++) {
        if (is_operand[i])
            continue;  // Skip operands
        
        int op = vm->text_seg[i];
        if (op == CALL || op == JMP || op == JZ || op == JNZ) {
            // Next value is a relative offset - convert to absolute address
            if (i + 1 < num_instructions) {
                long long offset = vm->text_seg[i + 1];
                // Convert relative offset to absolute address
                // Offsets should be small non-negative values (instruction indices)
                if (offset >= 0 && offset < num_instructions) {
                    long long addr = (long long)(vm->text_seg + offset);
                    vm->text_seg[i + 1] = addr;
                }
            }
        }
    }
    free(is_operand);
    
    if (vm->debug_vm) {
        printf("Loaded bytecode (%d bytes):\n", (int)text_size);
        printf("  Text size: %lld bytes (%lld instructions)\n", 
               text_size, text_size / sizeof(long long));
        printf("  Data size: %lld bytes\n", data_size);
        printf("  Main offset: %lld\n", main_offset);
    }
    
    return 0;
}

int cc_load_bytecode(JCC *vm, const char *path) {
    if (!vm || !path) {
        fprintf(stderr, "error: invalid arguments to cc_load_bytecode\n");
        return -1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    const char *data = malloc(file_size);
    if (!data) {
        fprintf(stderr, "error: failed to allocate memory for bytecode\n");
        fclose(f);
        return -1;
    }
    if (fread((void*)data, 1, file_size, f) != file_size) {
        fprintf(stderr, "error: failed to read bytecode file\n");
        free((void*)data);
        fclose(f);
        return -1;
    }
    int result = load_bytecode(vm, data, file_size);
    free((void*)data);
    fclose(f);
    return result;
}

void cc_compile(JCC *vm, Obj *prog) {
    if (!vm) {
        error("VM instance is NULL");
    }
    
    // Initialize VM memory if not already done
    if (!vm->text_seg) {
        // allocate memory
        if (!(vm->text_seg = malloc(vm->poolsize * sizeof(long long)))) {
            error("could not malloc for text area");
        }
        if (!(vm->data_seg = malloc(vm->poolsize))) {
            error("could not malloc for data area");
        }
        if (!(vm->stack_seg = malloc(vm->poolsize * sizeof(long long)))) {
            error("could not malloc for stack area");
        }
        if (!(vm->heap_seg = malloc(vm->poolsize))) {
            error("could not malloc for heap area");
        }

        // Allocate shadow stack for CFI if enabled
        if (vm->flags & JCC_CFI) {
            if (!(vm->shadow_stack = malloc(vm->poolsize * sizeof(long long)))) {
                error("could not malloc for shadow stack (CFI)");
            }
        }

        memset(vm->text_seg, 0, vm->poolsize * sizeof(long long));
        memset(vm->data_seg, 0, vm->poolsize);
        memset(vm->stack_seg, 0, vm->poolsize * sizeof(long long));
        memset(vm->heap_seg, 0, vm->poolsize);

        if (vm->flags & JCC_CFI) {
            memset(vm->shadow_stack, 0, vm->poolsize * sizeof(long long));
        }

        vm->old_text_seg = vm->text_seg;
        vm->text_ptr = vm->text_seg;
        vm->data_ptr = vm->data_seg;  // Initialize data pointer to start of data segment
        vm->heap_ptr = vm->heap_seg;  // Initialize heap pointer to start of heap
        vm->heap_end = vm->heap_seg + vm->poolsize;  // End of heap segment
        vm->free_list = NULL;  // Initialize free list to empty
        
        // Initialize codegen state
        vm->current_codegen_fn = NULL;

        // Initialize source map for debugger (if enabled)
        if (vm->flags & JCC_ENABLE_DEBUGGER) {
            vm->dbg.source_map_capacity = 1024;  // Initial capacity
            vm->dbg.source_map = malloc(vm->dbg.source_map_capacity * sizeof(SourceMap));
            if (!vm->dbg.source_map) {
                error("could not malloc for source map");
            }
            vm->dbg.source_map_count = 0;
            vm->dbg.last_debug_file = NULL;
            vm->dbg.last_debug_line = -1;
            vm->dbg.num_debug_symbols = 0;
            vm->dbg.num_watchpoints = 0;
        }
    }

    // Store the merged program for variable lookup during codegen
    vm->globals = prog;
    
    // Generate bytecode from AST
    codegen(vm, prog);
    
}