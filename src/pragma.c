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

// Pragma macro compilation subsystem
// Compiles #pragma macro functions using JCC recursively

#include "./internal.h"

// Global context: current parent VM for pragma macro execution
// This is set before calling a pragma macro and cleared after.
// Since pragma macro calls are synchronous and don't nest, this is safe.
static JCC *current_pragma_parent_vm = NULL;

// Builtin function to get the current VM context (like stdin/stdout)
JCC* __jcc_get_vm(void) {
    return current_pragma_parent_vm;
}

// Register AST builder API functions as FFI functions in the macro VM
static void register_ast_api(JCC *macro_vm, JCC *parent_vm) {
    // Builtin VM accessor (like stdin/stdout in stdio.h)
    cc_register_cfunc(macro_vm, "__jcc_get_vm", (void*)__jcc_get_vm, 0, 0);
    // High-level API - Type lookup
    cc_register_cfunc(macro_vm, "ast_get_type", (void*)ast_get_type, 2, 0);
    cc_register_cfunc(macro_vm, "ast_find_type", (void*)ast_find_type, 2, 0);

    // High-level API - Literal constructors
    cc_register_cfunc(macro_vm, "ast_int_literal", (void*)ast_int_literal, 2, 0);
    cc_register_cfunc(macro_vm, "ast_string_literal", (void*)ast_string_literal, 2, 0);
    cc_register_cfunc(macro_vm, "ast_var_ref", (void*)ast_var_ref, 2, 0);

    // High-level API - Enum reflection
    cc_register_cfunc(macro_vm, "ast_enum_name", (void*)ast_enum_name, 1, 0);
    cc_register_cfunc(macro_vm, "ast_enum_value_count", (void*)ast_enum_value_count, 1, 0);
    cc_register_cfunc(macro_vm, "ast_enum_value_name", (void*)ast_enum_value_name, 2, 0);
    cc_register_cfunc(macro_vm, "ast_enum_value", (void*)ast_enum_value, 2, 0);

    // High-level API - Control flow
    cc_register_cfunc(macro_vm, "ast_switch", (void*)ast_switch, 2, 0);
    cc_register_cfunc(macro_vm, "ast_switch_add_case", (void*)ast_switch_add_case, 4, 0);
    cc_register_cfunc(macro_vm, "ast_switch_set_default", (void*)ast_switch_set_default, 3, 0);
    cc_register_cfunc(macro_vm, "ast_return", (void*)ast_return, 2, 0);

    // High-level API - Function construction
    cc_register_cfunc(macro_vm, "ast_function", (void*)ast_function, 3, 0);
    cc_register_cfunc(macro_vm, "ast_function_add_param", (void*)ast_function_add_param, 4, 0);
    cc_register_cfunc(macro_vm, "ast_function_set_body", (void*)ast_function_set_body, 3, 0);

    // High-level API - Struct construction
    cc_register_cfunc(macro_vm, "ast_struct", (void*)ast_struct, 2, 0);
    cc_register_cfunc(macro_vm, "ast_struct_add_field", (void*)ast_struct_add_field, 4, 0);

    // Low-level API - Type introspection
    cc_register_cfunc(macro_vm, "ast_type_kind", (void*)ast_type_kind, 1, 0);
    cc_register_cfunc(macro_vm, "ast_type_size", (void*)ast_type_size, 1, 0);
    cc_register_cfunc(macro_vm, "ast_type_name", (void*)ast_type_name, 1, 0);
    cc_register_cfunc(macro_vm, "ast_type_exists", (void*)ast_type_exists, 2, 0);

    // Low-level API - Enum introspection
    cc_register_cfunc(macro_vm, "ast_enum_count", (void*)ast_enum_count, 2, 0);
    cc_register_cfunc(macro_vm, "ast_enum_at", (void*)ast_enum_at, 3, 0);
    cc_register_cfunc(macro_vm, "ast_enum_find", (void*)ast_enum_find, 3, 0);
    cc_register_cfunc(macro_vm, "ast_enum_constant_name", (void*)ast_enum_constant_name, 1, 0);
    cc_register_cfunc(macro_vm, "ast_enum_constant_value", (void*)ast_enum_constant_value, 1, 0);

    // Low-level API - Node creation
    cc_register_cfunc(macro_vm, "ast_node_num", (void*)ast_node_num, 2, 0);
    cc_register_cfunc(macro_vm, "ast_node_float", (void*)ast_node_float, 2, 0);
    cc_register_cfunc(macro_vm, "ast_node_string", (void*)ast_node_string, 2, 0);
    cc_register_cfunc(macro_vm, "ast_node_ident", (void*)ast_node_ident, 2, 0);
    cc_register_cfunc(macro_vm, "ast_node_binary", (void*)ast_node_binary, 4, 0);
    cc_register_cfunc(macro_vm, "ast_node_unary", (void*)ast_node_unary, 3, 0);
    cc_register_cfunc(macro_vm, "ast_node_block", (void*)ast_node_block, 3, 0);
    cc_register_cfunc(macro_vm, "ast_node_call", (void*)ast_node_call, 4, 0);
    cc_register_cfunc(macro_vm, "ast_node_member", (void*)ast_node_member, 3, 0);
    cc_register_cfunc(macro_vm, "ast_node_cast", (void*)ast_node_cast, 3, 0);
}

// Compile a single pragma macro
static bool compile_single_pragma_macro(JCC *parent_vm, PragmaMacro *pm) {
    // Create a new VM instance for compiling this macro
    JCC macro_vm;
    cc_init(&macro_vm, false);

    // Copy important state from parent VM
    macro_vm.debug_vm = 0;  // Disable debug for clean pragma macro compilation
    macro_vm.compiling_pragma_macro = true;  // Skip main() requirement

    // Copy include paths from parent VM
    for (int i = 0; i < parent_vm->include_paths.len; i++) {
        cc_include(&macro_vm, parent_vm->include_paths.data[i]);
    }

    // Add "./include" path if not already there (for pragma_api.h)
    cc_include(&macro_vm, "./include");

    // Register AST builder API
    register_ast_api(&macro_vm, parent_vm);

    // Also load standard library for convenience functions
    cc_load_stdlib(&macro_vm);

    // Prepend #include "pragma_api.h" to the token stream
    // Create a synthetic include directive
    const char *include_pragma_api = "#include \"pragma_api.h\"\n";
    Token *include_tok = tokenize(&macro_vm, new_file("<pragma-macro-header>", 1, (char*)include_pragma_api));

    // Link the include tokens to the body tokens
    Token *last_include_tok = include_tok;
    while (last_include_tok->next && last_include_tok->next->kind != TK_EOF)
        last_include_tok = last_include_tok->next;
    last_include_tok->next = pm->body_tokens;

    // Preprocess the combined token stream
    Token *preprocessed_tokens = preprocess(&macro_vm, include_tok);

    // Parse the pragma macro function
    Obj *prog = cc_parse(&macro_vm, preprocessed_tokens);
    if (!prog) {
        if (parent_vm->debug_vm)
            fprintf(stderr, "Failed to parse pragma macro '%s'\n", pm->name);
        cc_destroy(&macro_vm);
        return false;
    }
    
    // Compile it
    cc_compile(&macro_vm, prog);

    // Find the function in the compiled program
    Obj *func = NULL;
    for (Obj *obj = macro_vm.globals; obj; obj = obj->next) {
        if (obj->is_function && strcmp(obj->name, pm->name) == 0) {
            func = obj;
            break;
        }
    }
    
    if (!func) {
        if (parent_vm->debug_vm)
            fprintf(stderr, "Could not find pragma macro function '%s' after compilation\n", pm->name);
        cc_destroy(&macro_vm);
        return false;
    }
    
    // Store the compiled function address and VM
    pm->compiled_fn = (void*)(long long)func->code_addr;
    pm->macro_vm = calloc(1, sizeof(JCC));
    memcpy(pm->macro_vm, &macro_vm, sizeof(JCC));

    // Initialize the stack pointers for macro execution
    pm->macro_vm->sp = (long long *)((char *)pm->macro_vm->stack_seg + pm->macro_vm->poolsize * sizeof(long long));
    pm->macro_vm->bp = pm->macro_vm->sp;
    pm->macro_vm->initial_sp = pm->macro_vm->sp;
    pm->macro_vm->initial_bp = pm->macro_vm->bp;

    // Setup shadow stack for CFI if enabled
    if (pm->macro_vm->flags & JCC_CFI) {
        pm->macro_vm->shadow_sp = (long long *)((char *)pm->macro_vm->shadow_stack + pm->macro_vm->poolsize * sizeof(long long));
    }

    if (parent_vm->debug_vm)
        printf("Compiled pragma macro '%s' at address %lld\n", pm->name, func->code_addr);
    
    return true;
}

// Compile all pragma macros extracted during preprocessing
void compile_pragma_macros(JCC *vm) {
    if (!vm)
        return;
    
    if (vm->debug_vm && vm->pragma_macros)
        printf("Compiling pragma macros...\n");
    
    for (PragmaMacro *pm = vm->pragma_macros; pm; pm = pm->next) {
        if (!compile_single_pragma_macro(vm, pm)) {
            fprintf(stderr, "Warning: Failed to compile pragma macro '%s'\n", pm->name);
        }
    }
}

// Find a pragma macro by name
PragmaMacro *find_pragma_macro(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;
    
    for (PragmaMacro *pm = vm->pragma_macros; pm; pm = pm->next) {
        if (strcmp(pm->name, name) == 0)
            return pm;
    }
    return NULL;
}

// Expand pragma macro calls in token stream (for -E output)
// Returns a new token stream with macro calls replaced by serialized AST
Token *expand_pragma_macro_calls(JCC *vm, Token *tok) {
    if (!vm || !tok || !vm->pragma_macros)
        return tok;  // No macros to expand

    // Create a dummy head for the new token list
    Token head = {};
    Token *cur = &head;

    Token *t = tok;
    while (t && t->kind != TK_EOF) {
        // Check if this is a pragma macro call: identifier followed by '('
        if (t->kind == TK_IDENT && t->next && equal(t->next, "(")) {
            char *name = strndup(t->loc, t->len);
            PragmaMacro *pm = find_pragma_macro(vm, name);

            if (pm) {
                // Found a pragma macro call
                if (vm->debug_vm)
                    printf("Expanding pragma macro '%s' in token stream...\n", name);

                Token *call_start = t;
                t = t->next->next;  // Skip identifier and '('

                // Parse arguments
                Node **args = NULL;
                int arg_count = 0;
                int capacity = 0;

                // Handle empty argument list
                if (equal(t, ")")) {
                    t = t->next;  // Skip ')'
                } else {
                    // Parse arguments until we find the closing ')'
                    while (!equal(t, ")") && t->kind != TK_EOF) {
                        // Find the end of this argument by tracking parenthesis depth
                        Token *arg_start = t;
                        int depth = 0;
                        Token *arg_end = t;

                        while (arg_end && arg_end->kind != TK_EOF) {
                            if (equal(arg_end, "("))
                                depth++;
                            else if (equal(arg_end, ")")) {
                                if (depth == 0)
                                    break;  // Found closing paren of call
                                depth--;
                            } else if (equal(arg_end, ",") && depth == 0) {
                                break;  // Found argument separator
                            }
                            arg_end = arg_end->next;
                        }

                        // Parse this argument expression (use assign to stop at commas)
                        Token *arg_rest;
                        Node *arg = cc_parse_assign(vm, &arg_rest, arg_start);

                        if (vm->debug_vm)
                            printf("  Parsed argument %d for '%s': kind=%d\n", arg_count, name, arg ? arg->kind : -1);

                        // Add to args array
                        if (arg_count >= capacity) {
                            capacity = capacity == 0 ? 4 : capacity * 2;
                            args = realloc(args, capacity * sizeof(Node*));
                        }
                        args[arg_count++] = arg;

                        // Move past this argument
                        t = arg_rest;
                        if (equal(t, ","))
                            t = t->next;  // Skip comma
                    }

                    if (equal(t, ")"))
                        t = t->next;  // Skip ')'
                }

                // Execute the macro
                Node *result = execute_pragma_macro(vm, pm, args, arg_count);

                if (result) {
                    // Serialize the result AST to source text
                    char *serialized = serialize_node_to_source(vm, result);

                    if (serialized && serialized[0]) {
                        // Create a new token for the serialized output
                        Token *new_tok = calloc(1, sizeof(Token));
                        new_tok->kind = TK_IDENT;  // Will be parsed later
                        new_tok->loc = serialized;
                        new_tok->len = strlen(serialized);
                        new_tok->has_space = call_start->has_space;
                        new_tok->at_bol = call_start->at_bol;

                        // Add to output list
                        cur->next = new_tok;
                        cur = new_tok;
                    }
                    // Note: serialized string is now owned by the token
                }

                // Clean up
                free(args);
                free(name);
                continue;
            }
            free(name);
        }

        // Not a macro call, copy token as-is
        Token *copy = calloc(1, sizeof(Token));
        *copy = *t;
        copy->next = NULL;
        cur->next = copy;
        cur = copy;
        t = t->next;
    }

    // Add EOF token
    Token *eof = calloc(1, sizeof(Token));
    eof->kind = TK_EOF;
    cur->next = eof;

    return head.next;
}

// Execute a pragma macro and return the generated AST node
// This is called during parsing when a macro call site is detected
Node *execute_pragma_macro(JCC *vm, PragmaMacro *pm, Node **args, int arg_count) {
    if (!vm || !pm || !pm->compiled_fn || !pm->macro_vm)
        return NULL;

    // Save current VM state
    JCC *macro_vm = pm->macro_vm;
    long long saved_pc = (long long)macro_vm->pc;
    long long saved_sp = (long long)macro_vm->sp;
    long long saved_bp = (long long)macro_vm->bp;
    long long saved_ax = macro_vm->ax;
    int saved_debug = macro_vm->debug_vm;

    // Disable debug output during macro execution (for clean -E output)
    macro_vm->debug_vm = 0;

    // Set the parent VM context so __jcc_get_vm() can access it
    current_pragma_parent_vm = vm;

    // Set up for function call
    // pm->compiled_fn is an offset, need to add text_seg base
    long long func_offset = (long long)pm->compiled_fn;
    macro_vm->pc = macro_vm->text_seg + func_offset;
    macro_vm->sp = macro_vm->initial_sp;
    macro_vm->bp = macro_vm->initial_bp;

    // Note: We no longer push the parent VM pointer as an argument
    // Users access it via __VM (which calls __jcc_get_vm())

    // Push arguments onto the stack
    for (int i = arg_count - 1; i >= 0; i--) {
        *(--macro_vm->sp) = (long long)args[i];
    }

    // Push a sentinel return address (NULL) so LEV can detect when function returns
    *(--macro_vm->sp) = 0;

    // Execute the function
    vm_eval(macro_vm);

    // Get the returned Node* from ax register
    Node *generated_node = (Node*)(long long)macro_vm->ax;

    // Clear the parent VM context
    current_pragma_parent_vm = NULL;

    // Restore VM state (in case we call again)
    macro_vm->pc = (long long*)saved_pc;
    macro_vm->sp = (long long*)saved_sp;
    macro_vm->bp = (long long*)saved_bp;
    macro_vm->ax = saved_ax;
    macro_vm->debug_vm = saved_debug;

    if (vm->debug_vm && generated_node)
        printf("Pragma macro '%s' generated AST node of kind %d\n", pm->name, generated_node->kind);

    return generated_node;
}

