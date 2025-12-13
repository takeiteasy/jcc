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

// Bytecode serialization DISABLED - needs update for register-based opcodes
// TODO: Reimplement save/load for new instruction encoding

int cc_save_bytecode(JCC *vm, const char *path) {
    (void)vm; (void)path;
    fprintf(stderr, "error: bytecode serialization not yet implemented for register-based VM\n");
    return -1;
}

int cc_load_bytecode(JCC *vm, const char *path) {
    (void)vm; (void)path;
    fprintf(stderr, "error: bytecode serialization not yet implemented for register-based VM\n");
    return -1;
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
        vm->data_ptr = vm->data_seg;
        vm->heap_ptr = vm->heap_seg;
        vm->heap_end = vm->heap_seg + vm->poolsize;
        vm->free_list = NULL;
        
        // Initialize codegen state
        vm->compiler.current_codegen_fn = NULL;

        // Initialize source map for debugger (if enabled)
        if (vm->flags & JCC_ENABLE_DEBUGGER) {
            vm->dbg.source_map_capacity = 1024;
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
    vm->compiler.globals = prog;
    
    // Generate bytecode from AST using new register-based codegen
    gen(vm, prog);

    // Run optimizer if enabled
    if (vm->compiler.opt_level > 0) {
        cc_optimize(vm, vm->compiler.opt_level);
    }
}