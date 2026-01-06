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

// Reflection API for pragma macros
// Provides type introspection and AST construction functions

#include "./internal.h"

// Global VM pointer for __jcc_get_vm() builtin
// Set during pragma macro execution, cleared after
JCC *__jcc_current_vm = NULL;

// Builtin function to get the current VM context
JCC *__jcc_get_vm(void) { return __jcc_current_vm; }

// ============================================================================
// Internal Helpers (replicate static functions from parse.c)
// ============================================================================

static Obj *reflect_new_var(JCC *vm, char *name, int name_len, Type *ty) {
    Obj *var = arena_alloc(&vm->compiler.parser_arena, sizeof(Obj));
    memset(var, 0, sizeof(Obj));
    var->name = name;
    var->ty = ty;
    var->align = ty->align;
    return var;
}

static char *reflect_unique_name(JCC *vm) {
    char *name = format(".L..%d", vm->compiler.unique_name_counter++);
    strarray_push(&vm->compiler.file_buffers, name);
    return name;
}

static Obj *reflect_new_gvar(JCC *vm, char *name, int name_len, Type *ty) {
    Obj *var = reflect_new_var(vm, name, name_len, ty);
    var->next = vm->compiler.globals;
    var->is_static = true;
    var->is_definition = true;
    vm->compiler.globals = var;
    return var;
}

static Obj *reflect_new_anon_gvar(JCC *vm, Type *ty) {
    char *name = reflect_unique_name(vm);
    return reflect_new_gvar(vm, name, strlen(name), ty);
}

// ============================================================================
// Type Lookup and Introspection
// ============================================================================

Type *ast_find_type(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;

    size_t name_len = strlen(name);

    // Search through all scopes, starting from innermost
    for (Scope *sc = vm->compiler.scope; sc; sc = sc->next) {
        // First search struct/union/enum tags
        for (TagScopeNode *node = sc->tags; node; node = node->next) {
            if (node->name_len == (int)name_len &&
                strncmp(node->name, name, name_len) == 0) {
                return node->ty;
            }
        }
        // Also search typedefs (stored in vars with type_def set)
        for (VarScopeNode *node = sc->vars; node; node = node->next) {
            if (node->type_def && node->name_len == (int)name_len &&
                strncmp(node->name, name, name_len) == 0) {
                return node->type_def;
            }
        }
    }

    return NULL;
}

bool ast_type_exists(JCC *vm, const char *name) {
    return ast_find_type(vm, name) != NULL;
}

Type *ast_get_type(JCC *vm, const char *name) {
    // For built-in types
    if (!name)
        return NULL;

    if (strcmp(name, "void") == 0)
        return ty_void;
    if (strcmp(name, "char") == 0)
        return ty_char;
    if (strcmp(name, "short") == 0)
        return ty_short;
    if (strcmp(name, "int") == 0)
        return ty_int;
    if (strcmp(name, "long") == 0)
        return ty_long;
    if (strcmp(name, "float") == 0)
        return ty_float;
    if (strcmp(name, "double") == 0)
        return ty_double;
    if (strcmp(name, "_Bool") == 0)
        return ty_bool;

    // For user-defined types
    return ast_find_type(vm, name);
}

TypeKind ast_type_kind(Type *ty) { return ty ? ty->kind : TY_VOID; }

int ast_type_size(Type *ty) { return ty ? ty->size : 0; }

int ast_type_align(Type *ty) { return ty ? ty->align : 0; }

bool ast_type_is_unsigned(Type *ty) { return ty ? ty->is_unsigned : false; }

bool ast_type_is_const(Type *ty) { return ty ? ty->is_const : false; }

Type *ast_type_base(Type *ty) {
    if (!ty)
        return NULL;
    if (ty->kind != TY_PTR && ty->kind != TY_ARRAY && ty->kind != TY_VLA)
        return NULL;
    return ty->base;
}

int ast_type_array_len(Type *ty) {
    if (!ty || ty->kind != TY_ARRAY)
        return -1;
    return ty->array_len;
}

Type *ast_type_return_type(Type *ty) {
    if (!ty || ty->kind != TY_FUNC)
        return NULL;
    return ty->return_ty;
}

int ast_type_param_count(Type *ty) {
    if (!ty || ty->kind != TY_FUNC)
        return -1;

    int count = 0;
    for (Type *p = ty->params; p; p = p->next)
        count++;
    return count;
}

Type *ast_type_param_at(Type *ty, int index) {
    if (!ty || ty->kind != TY_FUNC || index < 0)
        return NULL;

    Type *p = ty->params;
    for (int i = 0; i < index && p; i++)
        p = p->next;
    return p;
}

bool ast_type_is_variadic(Type *ty) {
    if (!ty || ty->kind != TY_FUNC)
        return false;
    return ty->is_variadic;
}

const char *ast_type_name(Type *ty) {
    if (!ty || !ty->name)
        return NULL;

    // Extract string from token
    static char buffer[256];
    int len = ty->name->len;
    if (len >= (int)sizeof(buffer))
        len = sizeof(buffer) - 1;
    strncpy(buffer, ty->name->loc, len);
    buffer[len] = '\0';
    return buffer;
}

Type *ast_make_pointer(JCC *vm, Type *base) {
    if (!vm || !base)
        return NULL;
    return pointer_to(vm, base);
}

Type *ast_make_array(JCC *vm, Type *base, int len) {
    if (!vm || !base || len < 0)
        return NULL;
    return array_of(vm, base, len);
}

// ============================================================================
// Enum Reflection
// ============================================================================

int ast_enum_count(JCC *vm, Type *enum_type) {
    (void)vm; // Unused but kept for API consistency
    if (!enum_type || enum_type->kind != TY_ENUM)
        return -1;

    int count = 0;
    for (EnumConstant *ec = enum_type->enum_constants; ec; ec = ec->next)
        count++;
    return count;
}

EnumConstant *ast_enum_at(JCC *vm, Type *enum_type, int index) {
    (void)vm;
    if (!enum_type || enum_type->kind != TY_ENUM || index < 0)
        return NULL;

    EnumConstant *ec = enum_type->enum_constants;
    for (int i = 0; i < index && ec; i++)
        ec = ec->next;
    return ec;
}

EnumConstant *ast_enum_find(JCC *vm, Type *enum_type, const char *name) {
    (void)vm;
    if (!enum_type || enum_type->kind != TY_ENUM || !name)
        return NULL;

    for (EnumConstant *ec = enum_type->enum_constants; ec; ec = ec->next)
        if (strcmp(ec->name, name) == 0)
            return ec;
    return NULL;
}

const char *ast_enum_constant_name(EnumConstant *ec) {
    return ec ? ec->name : NULL;
}

int ast_enum_constant_value(EnumConstant *ec) { return ec ? ec->value : 0; }

const char *ast_enum_name(Type *e) { return ast_type_name(e); }

int ast_enum_value_count(Type *e) {
    int count = ast_enum_count(NULL, e);
    return count < 0 ? 0 : count;
}

const char *ast_enum_value_name(Type *e, int index) {
    EnumConstant *ec = ast_enum_at(NULL, e, index);
    return ast_enum_constant_name(ec);
}

int ast_enum_value(Type *e, int index) {
    EnumConstant *ec = ast_enum_at(NULL, e, index);
    return ast_enum_constant_value(ec);
}

// ============================================================================
// Struct/Union Member Introspection
// ============================================================================

int ast_struct_member_count(JCC *vm, Type *struct_type) {
    (void)vm;
    if (!struct_type)
        return -1;
    if (struct_type->kind != TY_STRUCT && struct_type->kind != TY_UNION)
        return -1;

    int count = 0;
    for (Member *m = struct_type->members; m; m = m->next)
        count++;
    return count;
}

Member *ast_struct_member_at(JCC *vm, Type *struct_type, int index) {
    (void)vm;
    if (!struct_type || index < 0)
        return NULL;
    if (struct_type->kind != TY_STRUCT && struct_type->kind != TY_UNION)
        return NULL;

    Member *m = struct_type->members;
    for (int i = 0; i < index && m; i++)
        m = m->next;
    return m;
}

Member *ast_struct_member_find(JCC *vm, Type *struct_type, const char *name) {
    (void)vm;
    if (!struct_type || !name)
        return NULL;
    if (struct_type->kind != TY_STRUCT && struct_type->kind != TY_UNION)
        return NULL;

    size_t name_len = strlen(name);
    for (Member *m = struct_type->members; m; m = m->next)
        if (m->name && m->name->len == (int)name_len &&
            strncmp(m->name->loc, name, name_len) == 0)
            return m;
    return NULL;
}

const char *ast_member_name(Member *m) {
    if (!m || !m->name)
        return NULL;

    // Extract string from token
    static char buffer[256];
    int len = m->name->len;
    if (len >= (int)sizeof(buffer))
        len = sizeof(buffer) - 1;
    strncpy(buffer, m->name->loc, len);
    buffer[len] = '\0';
    return buffer;
}

Type *ast_member_type(Member *m) { return m ? m->ty : NULL; }

int ast_member_offset(Member *m) { return m ? m->offset : 0; }

bool ast_member_is_bitfield(Member *m) { return m ? m->is_bitfield : false; }

int ast_member_bitfield_width(Member *m) {
    return (m && m->is_bitfield) ? m->bit_width : 0;
}

// ============================================================================
// Global Symbol Introspection
// ============================================================================

Obj *ast_find_global(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;

    size_t name_len = strlen(name);
    for (Obj *obj = vm->compiler.globals; obj; obj = obj->next) {
        if (strlen(obj->name) == name_len && strcmp(obj->name, name) == 0)
            return obj;
    }
    return NULL;
}

int ast_global_count(JCC *vm) {
    if (!vm)
        return 0;

    int count = 0;
    for (Obj *obj = vm->compiler.globals; obj; obj = obj->next)
        count++;
    return count;
}

Obj *ast_global_at(JCC *vm, int index) {
    if (!vm || index < 0)
        return NULL;

    Obj *obj = vm->compiler.globals;
    for (int i = 0; i < index && obj; i++)
        obj = obj->next;
    return obj;
}

const char *ast_obj_name(Obj *obj) { return obj ? obj->name : NULL; }

Type *ast_obj_type(Obj *obj) { return obj ? obj->ty : NULL; }

bool ast_obj_is_function(Obj *obj) { return obj ? obj->is_function : false; }

bool ast_obj_is_definition(Obj *obj) {
    return obj ? obj->is_definition : false;
}

bool ast_obj_is_static(Obj *obj) { return obj ? obj->is_static : false; }

// ============================================================================
// AST Node Construction - Helper
// ============================================================================

static Node *alloc_node(JCC *vm, NodeKind kind) {
    Node *node = arena_alloc(&vm->compiler.parser_arena, sizeof(Node));
    memset(node, 0, sizeof(Node));
    node->kind = kind;
    return node;
}

// ============================================================================
// AST Node Construction - Literals
// ============================================================================

Node *ast_int_literal(JCC *vm, int64_t value) {
    if (!vm)
        return NULL;

    Node *node = alloc_node(vm, ND_NUM);
    node->val = value;
    node->ty = ty_long;
    return node;
}

Node *ast_float_literal(JCC *vm, double value) {
    if (!vm)
        return NULL;

    Node *node = alloc_node(vm, ND_NUM);
    node->fval = value;
    node->ty = ty_double;
    return node;
}

Node *ast_string_literal(JCC *vm, const char *str) {
    if (!vm || !str)
        return NULL;

    int len = strlen(str);

    // Create type for the string
    Type *ty = array_of(vm, ty_char, len + 1);

    // Create an anonymous global variable for the string
    Obj *var = reflect_new_anon_gvar(vm, ty);

    // Allocate space in the data segment and copy the string data
    // This is critical: we must place the data in the segment NOW,
    // not just set init_data (which is only used during initial emit_program)
    long long offset = vm->data_ptr - vm->data_seg;
    offset = (offset + 7) & ~7; // Align to 8 bytes
    vm->data_ptr = vm->data_seg + offset;

    var->offset = offset;
    var->init_data = (char *)vm->data_ptr; // Point directly to data segment

    // Copy string to data segment
    memcpy(vm->data_ptr, str, len + 1);
    vm->data_ptr += len + 1;

    // Create a variable reference node
    Node *node = alloc_node(vm, ND_VAR);
    node->var = var;
    node->ty = ty;
    return node;
}

Node *ast_var_ref(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;

    // Look up the variable in current scope
    size_t name_len = strlen(name);

    for (Scope *sc = vm->compiler.scope; sc; sc = sc->next) {
        for (VarScopeNode *node = sc->vars; node; node = node->next) {
            if (node->name_len == (int)name_len &&
                strncmp(node->name, name, name_len) == 0) {
                if (node->var) {
                    Node *n = alloc_node(vm, ND_VAR);
                    n->var = node->var;
                    n->ty = node->var->ty;
                    return n;
                }
            }
        }
    }

    // Also check globals
    Obj *global = ast_find_global(vm, name);
    if (global) {
        Node *n = alloc_node(vm, ND_VAR);
        n->var = global;
        n->ty = global->ty;
        return n;
    }

    return NULL;
}

// ============================================================================
// AST Node Construction - Expressions
// ============================================================================

Node *ast_binary(JCC *vm, NodeKind op, Node *left, Node *right) {
    if (!vm || !left || !right)
        return NULL;

    Node *node = alloc_node(vm, op);
    node->lhs = left;
    node->rhs = right;
    // Type will be determined by add_type pass
    return node;
}

Node *ast_unary(JCC *vm, NodeKind op, Node *operand) {
    if (!vm || !operand)
        return NULL;

    Node *node = alloc_node(vm, op);
    node->lhs = operand;
    return node;
}

Node *ast_cast(JCC *vm, Node *expr, Type *target_type) {
    if (!vm || !expr || !target_type)
        return NULL;

    Node *node = alloc_node(vm, ND_CAST);
    node->lhs = expr;
    node->ty = target_type;
    return node;
}

// ============================================================================
// AST Node Construction - Statements
// ============================================================================

Node *ast_return(JCC *vm, Node *expr) {
    if (!vm)
        return NULL;

    Node *node = alloc_node(vm, ND_RETURN);
    node->lhs = expr;
    return node;
}

Node *ast_block(JCC *vm, Node **stmts, int count) {
    if (!vm)
        return NULL;

    Node *node = alloc_node(vm, ND_BLOCK);

    // Link statements together
    Node head = {};
    Node *cur = &head;
    for (int i = 0; i < count && stmts[i]; i++) {
        cur = cur->next = stmts[i];
    }
    node->body = head.next;
    return node;
}

Node *ast_if(JCC *vm, Node *cond, Node *then_body, Node *else_body) {
    if (!vm || !cond)
        return NULL;

    Node *node = alloc_node(vm, ND_IF);
    node->cond = cond;
    node->then = then_body;
    node->els = else_body;
    return node;
}

Node *ast_switch(JCC *vm, Node *cond) {
    if (!vm || !cond)
        return NULL;

    Node *node = alloc_node(vm, ND_SWITCH);
    node->cond = cond;
    node->case_next = NULL;
    node->default_case = NULL;
    return node;
}

void ast_switch_add_case(JCC *vm, Node *switch_node, Node *value, Node *body) {
    if (!vm || !switch_node || !value || !body)
        return;

    if (switch_node->kind != ND_SWITCH)
        return;

    // Create a case node
    Node *case_node = alloc_node(vm, ND_CASE);
    case_node->begin = value->val; // Assuming value is a numeric literal
    case_node->end = value->val;
    case_node->body = body;

    // Add to switch's case list
    case_node->case_next = switch_node->case_next;
    switch_node->case_next = case_node;
}

void ast_switch_set_default(JCC *vm, Node *switch_node, Node *body) {
    if (!vm || !switch_node || !body)
        return;

    if (switch_node->kind != ND_SWITCH)
        return;

    Node *def = alloc_node(vm, ND_CASE);
    def->body = body;
    switch_node->default_case = def;
}

// ============================================================================
// AST Node Construction - Declarations
// ============================================================================

// Note: Full function/struct generation is complex and will be implemented
// in a later phase. For now, we provide the basics for expression generation.

Node *ast_expr_stmt(JCC *vm, Node *expr) {
    if (!vm)
        return NULL;

    Node *node = alloc_node(vm, ND_EXPR_STMT);
    node->lhs = expr;
    return node;
}
