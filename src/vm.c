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

#define X(NAME) extern int op_##NAME##_fn(JCC *vm);
OPS_X
#undef X

static int eval1(JCC *vm) {
    vm->cycle++;

    // Debugger hooks - check before executing instruction
    if (vm->flags & JCC_ENABLE_DEBUGGER) {
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

    static void* op_table[] = {
#define X(NAME) [NAME] = &&op_##NAME,
        OPS_X
#undef X
    };
    int op = *vm->pc++;
    if (op < 0 || op >= sizeof(op_table) / sizeof(op_table[0])) {
        printf("unknown instruction:%d\n", op);
        return -1;
    }

    // Debug printing
    if (vm->debug_vm) {
        printf("%lld> %.5s", vm->cycle,
                &"LEA  ,IMM  ,JMP  ,CALL ,CALI ,JZ   ,JNZ  ,ENT  ,ADJ  ,LEV  ,LI   ,LC   ,LS   ,LW   ,SI   ,SC   ,SS   ,SW   ,PUSH ,"
                "OR   ,XOR  ,AND  ,EQ   ,NE   ,LT   ,GT   ,LE   ,GE   ,SHL  ,SHR  ,ADD  ,SUB  ,MUL  ,DIV  ,MOD  ,"
                "MALC ,MFRE ,MCPY ,"
                "SX1  ,SX2  ,SX4  ,ZX1  ,ZX2  ,ZX4  ,"
                "FLD  ,FST  ,FADD ,FSUB ,FMUL ,FDIV ,FNEG ,FEQ  ,FNE  ,FLT  ,FLE  ,FGT  ,FGE  ,I2F  ,F2I  ,FPSH ,"
                "CALLF,CHKB ,CHKP ,CHKT ,CHKI ,MARKI,MARKA,CHKA ,CHKPA,MARKP,"
                "SCPIN,SCPOT,CHKL ,MARKR,MARKW,"
                "SETJP,LONJP"[op * 6]);
        if (op <= ADJ || op == CHKB || op == CHKT || op == CHKI || op == MARKI || op == MARKA || op == CHKA || op == MARKP || op == SCOPEIN || op == SCOPEOUT || op == CHKL || op == MARKR || op == MARKW)
            printf(" %lld\n", *vm->pc);
        else
            printf("\n");
    }

    goto *op_table[op];
#define X(NAME) op_##NAME: return op_##NAME##_fn(vm);
    OPS_X 
#undef X
    return -1;
}

int vm_eval(JCC *vm) {
    int result = 0;
    vm->cycle = 0;
    while ((result = eval1(vm)) == 0) {
        // Check if program has exited (pc set to NULL by LEV)
        if (vm->pc == NULL || vm->pc == 0) {
            return (int)vm->ax;
        }
    }
    return result;
}

void cc_init(JCC *vm, uint32_t flags) {
    // Zero-initialize the VM struct
    memset(vm, 0, sizeof(JCC));

    // Set runtime flags
    vm->flags = flags;

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

    // Initialize sorted allocation array for O(log n) pointer validation
    vm->sorted_allocs.addresses = NULL;
    vm->sorted_allocs.headers = NULL;
    vm->sorted_allocs.count = 0;
    vm->sorted_allocs.capacity = 0;

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
    // The flag JCC_RANDOM_CANARIES will trigger regeneration in main.c
    // For now, initialize to fixed value; it will be regenerated if random canaries enabled
    vm->stack_canary = STACK_CANARY;

    // Add default system include path for <...> includes
    cc_system_include(vm, "./include");

    // If VM was built with libffi support, define JCC_HAS_FFI for user code
#ifdef JCC_HAS_FFI
    cc_define(vm, "JCC_HAS_FFI", "1");
#endif

    if (vm->flags & JCC_ENABLE_DEBUGGER) {
        debugger_init(vm);
    }
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

    // Free sorted allocation arrays
    if (vm->sorted_allocs.addresses)
        free(vm->sorted_allocs.addresses);
    if (vm->sorted_allocs.headers)
        free(vm->sorted_allocs.headers);

    // Free macros HashMap (string keys from tokens - not allocated, but Macro values are allocated)
    if (vm->macros.buckets) {
        for (int i = 0; i < vm->macros.capacity; i++) {
            HashEntry *entry = &vm->macros.buckets[i];
            if (entry->key && entry->key != (void *)-1) {
                // Don't free key - it points to token loc, not allocated by HashMap
                // Free Macro value (Macro struct is defined in preprocess.c, treat as opaque)
                // Note: Internal Macro fields (params, body tokens) are acceptable leaks
                // for short-lived compiler invocations, or should be freed in preprocess.c cleanup
                if (entry->val) {
                    free(entry->val);
                }
            }
        }
        free(vm->macros.buckets);
    }

    // Free pragma_once HashMap (string keys from file names - allocated, values are just (void*)1)
    if (vm->pragma_once.buckets) {
        for (int i = 0; i < vm->pragma_once.capacity; i++) {
            HashEntry *entry = &vm->pragma_once.buckets[i];
            if (entry->key && entry->key != (void *)-1 && entry->keylen != -1) {
                // Free string key (file name)
                free(entry->key);
            }
        }
        free(vm->pragma_once.buckets);
    }

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

void cc_print_stack_report(JCC *vm) {
    if (!vm || !(vm->flags & JCC_STACK_INSTR)) {
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

    // Get entry point (main function) from text_seg[0]
    long long main_addr = vm->text_seg[0];

    // main_addr is an offset from text_seg
    vm->pc = vm->text_seg + main_addr;

    // Setup stack
    vm->sp = (long long *)((char *)vm->stack_seg + vm->poolsize * sizeof(long long));
    vm->bp = vm->sp;  // Initialize base pointer to top of stack

    // Setup shadow stack for CFI if enabled
    if (vm->flags & JCC_CFI) {
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

    return (vm->flags & JCC_ENABLE_DEBUGGER) ? debugger_run(vm, argc, argv) : vm_eval(vm);
}
