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

#include "./internal.h"

Type *ast_find_type(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;

    // Search through all scopes, starting from innermost
    for (Scope *sc = vm->scope; sc; sc = sc->next) {
        Type *ty = hashmap_get2(&sc->tags, (char*)name, strlen(name));
        if (ty)
            return ty;
    }

    return NULL;
}

bool ast_type_exists(JCC *vm, const char *name) {
    return ast_find_type(vm, name) != NULL;
}

int ast_enum_count(JCC *vm, Type *enum_type) {
    if (!enum_type || enum_type->kind != TY_ENUM) return -1;

    int count = 0;
    for (EnumConstant *ec = enum_type->enum_constants; ec; ec = ec->next)
        count++;
    return count;
}

EnumConstant *ast_enum_at(JCC *vm, Type *enum_type, int index) {
    if (!enum_type || enum_type->kind != TY_ENUM || index < 0)
        return NULL;

    EnumConstant *ec = enum_type->enum_constants;
    for (int i = 0; i < index && ec; i++)
        ec = ec->next;
    return ec;
}

EnumConstant *ast_enum_find(JCC *vm, Type *enum_type, const char *name) {
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

int ast_enum_constant_value(EnumConstant *ec) {
    return ec ? ec->value : 0;
}

int ast_struct_member_count(JCC *vm, Type *struct_type) {
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
    if (!struct_type || !name)
        return NULL;
    if (struct_type->kind != TY_STRUCT && struct_type->kind != TY_UNION)
        return NULL;

    for (Member *m = struct_type->members; m; m = m->next)
        if (m->name && m->name->len == strlen(name) &&
            strncmp(m->name->loc, name, m->name->len) == 0)
            return m;
    return NULL;
}

const char *ast_member_name(Member *m) {
    if (!m || !m->name)
        return NULL;

    // Extract string from token (need to null-terminate it)
    // Since jcc uses token->loc and token->len, we need to return a properly terminated string
    // For simplicity, we'll use strndup (or equivalent) here
    static char buffer[256];
    int len = m->name->len;
    if (len >= sizeof(buffer))
        len = sizeof(buffer) - 1;
    strncpy(buffer, m->name->loc, len);
    buffer[len] = '\0';
    return buffer;
}

Type *ast_member_type(Member *m) {
    return m ? m->ty : NULL;
}

int ast_member_offset(Member *m) {
    return m ? m->offset : 0;
}

bool ast_member_is_bitfield(Member *m) {
    return m ? m->is_bitfield : false;
}

int ast_member_bitfield_width(Member *m) {
    return (m && m->is_bitfield) ? m->bit_width : 0;
}

TypeKind ast_type_kind(Type *ty) {
    return ty ? ty->kind : TY_VOID;
}

int ast_type_size(Type *ty) {
    return ty ? ty->size : 0;
}

int ast_type_align(Type *ty) {
    return ty ? ty->align : 0;
}

bool ast_type_is_unsigned(Type *ty) {
    return ty ? ty->is_unsigned : false;
}

bool ast_type_is_const(Type *ty) {
    return ty ? ty->is_const : false;
}

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

    static char buffer[256];
    int len = ty->name->len;
    if (len >= sizeof(buffer))
        len = sizeof(buffer) - 1;
    strncpy(buffer, ty->name->loc, len);
    buffer[len] = '\0';
    return buffer;
}

// Type category checks
// Note: is_integer and is_flonum are already defined in type.c (declared in internal.h)
// We wrap them here for the public API

bool ast_is_integer(Type *ty) {
    return is_integer(ty);
}

bool ast_is_flonum(Type *ty) {
    return is_flonum(ty);
}

bool ast_is_pointer(Type *ty) {
    return ty && (ty->kind == TY_PTR || ty->kind == TY_ARRAY || ty->kind == TY_VLA);
}

bool ast_is_array(Type *ty) {
    return ty && (ty->kind == TY_ARRAY || ty->kind == TY_VLA);
}

bool ast_is_function(Type *ty) {
    return ty && ty->kind == TY_FUNC;
}

bool ast_is_struct(Type *ty) {
    return ty && ty->kind == TY_STRUCT;
}

bool ast_is_union(Type *ty) {
    return ty && ty->kind == TY_UNION;
}

bool ast_is_enum(Type *ty) {
    return ty && ty->kind == TY_ENUM;
}

Obj *ast_find_global(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;

    for (Obj *obj = vm->globals; obj; obj = obj->next)
        if (strcmp(obj->name, name) == 0)
            return obj;
    return NULL;
}

int ast_global_count(JCC *vm) {
    if (!vm)
        return 0;

    int count = 0;
    for (Obj *obj = vm->globals; obj; obj = obj->next)
        count++;
    return count;
}

Obj *ast_global_at(JCC *vm, int index) {
    if (!vm || index < 0)
        return NULL;

    Obj *obj = vm->globals;
    for (int i = 0; i < index && obj; i++)
        obj = obj->next;
    return obj;
}

const char *ast_obj_name(Obj *obj) {
    return obj ? obj->name : NULL;
}

Type *ast_obj_type(Obj *obj) {
    return obj ? obj->ty : NULL;
}

bool ast_obj_is_function(Obj *obj) {
    return obj ? obj->is_function : false;
}

bool ast_obj_is_definition(Obj *obj) {
    return obj ? obj->is_definition : false;
}

bool ast_obj_is_static(Obj *obj) {
    return obj ? obj->is_static : false;
}

int ast_func_param_count(Obj *func) {
    if (!func || !func->is_function)
        return -1;

    int count = 0;
    for (Obj *p = func->params; p; p = p->next)
        count++;
    return count;
}

Obj *ast_func_param_at(Obj *func, int index) {
    if (!func || !func->is_function || index < 0)
        return NULL;

    Obj *p = func->params;
    for (int i = 0; i < index && p; i++)
        p = p->next;
    return p;
}

Node *ast_func_body(Obj *func) {
    if (!func || !func->is_function)
        return NULL;
    return func->body;
}

static Node *alloc_node(JCC *vm, NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = NULL; // No source location for programmatically created nodes
    return node;
}

Node *ast_node_num(JCC *vm, long long value) {
    if (!vm)
        return NULL;

    Node *node = alloc_node(vm, ND_NUM);
    node->val = value;
    node->ty = ty_long;
    return node;
}

Node *ast_node_float(JCC *vm, double value) {
    if (!vm) return NULL;

    Node *node = alloc_node(vm, ND_NUM);
    node->fval = value;
    node->ty = ty_double;
    return node;
}

Node *ast_node_string(JCC *vm, const char *str) {
    if (!vm || !str)
        return NULL;

    // String literals are complex - they need to be added to the data segment
    // For now, we'll create a simplified version
    Node *node = alloc_node(vm, ND_NUM);
    // This is a placeholder - proper string literal creation would need more work
    return node;
}

Node *ast_node_ident(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;

    // Look up the variable
    Obj *var = ast_find_global(vm, name);
    if (!var)
        return NULL;

    Node *node = alloc_node(vm, ND_VAR);
    node->var = var;
    node->ty = var->ty;
    return node;
}

Node *ast_node_binary(JCC *vm, NodeKind op, Node *left, Node *right) {
    if (!vm || !left || !right)
        return NULL;

    Node *node = alloc_node(vm, op);
    node->lhs = left;
    node->rhs = right;
    // Type inference would happen in add_type() pass
    return node;
}

Node *ast_node_unary(JCC *vm, NodeKind op, Node *operand) {
    if (!vm || !operand)
        return NULL;

    Node *node = alloc_node(vm, op);
    node->lhs = operand;
    return node;
}

Node *ast_node_call(JCC *vm, const char *func_name, Node **args, int arg_count) {
    if (!vm || !func_name)
        return NULL;

    Node *node = alloc_node(vm, ND_FUNCALL);

    // Create identifier node for function name
    Obj *func = ast_find_global(vm, func_name);
    if (!func || !func->is_function)
        return NULL;

    Node *fn = ast_node_ident(vm, func_name);
    if (!fn)
        return NULL;

    node->lhs = fn;
    node->func_ty = func->ty;
    node->ty = func->ty->return_ty;

    // Link arguments
    if (args && arg_count > 0) {
        node->args = args[0];
        Node *cur = node->args;
        for (int i = 1; i < arg_count; i++) {
            cur->next = args[i];
            cur = cur->next;
        }
    }

    return node;
}

Node *ast_node_member(JCC *vm, Node *object, const char *member_name) {
    if (!vm || !object || !member_name)
        return NULL;

    // Get the struct/union type
    Type *ty = object->ty;
    if (!ty)
        return NULL;

    // Dereference if it's a pointer
    if (ty->kind == TY_PTR)
        ty = ty->base;

    if (ty->kind != TY_STRUCT && ty->kind != TY_UNION)
        return NULL;

    // Find the member
    Member *mem = ast_struct_member_find(vm, ty, member_name);
    if (!mem)
        return NULL;

    Node *node = alloc_node(vm, ND_MEMBER);
    node->lhs = object;
    node->member = mem;
    node->ty = mem->ty;
    return node;
}

Node *ast_node_cast(JCC *vm, Node *expr, Type *target_type) {
    if (!vm || !expr || !target_type)
        return NULL;
    return new_cast(vm, expr, target_type);
}

Node *ast_node_block(JCC *vm, Node **stmts, int stmt_count) {
    if (!vm)
        return NULL;

    Node *node = alloc_node(vm, ND_BLOCK);

    // Link statements
    if (stmts && stmt_count > 0) {
        node->body = stmts[0];
        Node *cur = node->body;
        for (int i = 1; i < stmt_count; i++) {
            cur->next = stmts[i];
            cur = cur->next;
        }
    }

    return node;
}

NodeKind ast_node_kind(Node *node) {
    return node ? node->kind : ND_NULL_EXPR;
}

Type *ast_node_type(Node *node) {
    return node ? node->ty : NULL;
}

long long ast_node_int_value(Node *node) {
    if (!node || node->kind != ND_NUM)
        return 0;
    return node->val;
}

double ast_node_float_value(Node *node) {
    if (!node || node->kind != ND_NUM)
        return 0.0;
    return node->fval;
}

const char *ast_node_string_value(Node *node) {
    // This would need to extract string from a string literal node
    // Placeholder for now
    return NULL;
}

Node *ast_node_left(Node *node) {
    return node ? node->lhs : NULL;
}

Node *ast_node_right(Node *node) {
    return node ? node->rhs : NULL;
}

const char *ast_node_func_name(Node *node) {
    if (!node || node->kind != ND_FUNCALL)
        return NULL;
    if (!node->lhs || node->lhs->kind != ND_VAR)
        return NULL;
    return ast_obj_name(node->lhs->var);
}

int ast_node_arg_count(Node *node) {
    if (!node || node->kind != ND_FUNCALL)
        return 0;

    int count = 0;
    for (Node *arg = node->args; arg; arg = arg->next)
        count++;
    return count;
}

Node *ast_node_arg_at(Node *node, int index) {
    if (!node || node->kind != ND_FUNCALL || index < 0)
        return NULL;

    Node *arg = node->args;
    for (int i = 0; i < index && arg; i++)
        arg = arg->next;
    return arg;
}

int ast_node_stmt_count(Node *node) {
    if (!node || node->kind != ND_BLOCK)
        return 0;

    int count = 0;
    for (Node *stmt = node->body; stmt; stmt = stmt->next)
        count++;
    return count;
}

Node *ast_node_stmt_at(Node *node, int index) {
    if (!node || node->kind != ND_BLOCK || index < 0)
        return NULL;

    Node *stmt = node->body;
    for (int i = 0; i < index && stmt; i++)
        stmt = stmt->next;
    return stmt;
}

Obj *ast_node_var(Node *node) {
    if (!node || node->kind != ND_VAR)
        return NULL;
    return node->var;
}

Token *ast_node_token(Node *node) {
    return node ? node->tok : NULL;
}

const char *ast_token_filename(Token *tok) {
    if (!tok)
        return NULL;
    return tok->filename ? tok->filename : (tok->file ? tok->file->name : NULL);
}

int ast_token_line(Token *tok) {
    return tok ? tok->line_no : 0;
}

int ast_token_column(Token *tok) {
    if (!tok) {
        return 0;
    }
    return tok->col_no;
}

int ast_token_text(Token *tok, char *buffer, int bufsize) {
    if (!tok || !buffer || bufsize <= 0)
        return 0;

    int len = tok->len;
    if (len >= bufsize)
        len = bufsize - 1;

    strncpy(buffer, tok->loc, len);
    buffer[len] = '\0';
    return len;
}

Type *ast_make_pointer(Type *base) {
    if (!base)
        return NULL;
    return pointer_to(base);
}

Type *ast_make_array(Type *base, int length) {
    if (!base || length < 0)
        return NULL;
    return array_of(base, length);
}

Type *ast_make_function(Type *return_type, Type **param_types, int param_count, bool is_variadic) {
    if (!return_type)
        return NULL;

    Type *ty = func_type(return_type);

    // Link parameter types
    if (param_types && param_count > 0) {
        ty->params = param_types[0];
        Type *cur = ty->params;
        for (int i = 1; i < param_count; i++) {
            cur->next = param_types[i];
            cur = cur->next;
        }
    }

    ty->is_variadic = is_variadic;
    return ty;
}

// ============================================================================
// High-Level AST Builder API for Pragma Macros
// ============================================================================

// Helper to lookup type by name (alias for ast_find_type)
Type *ast_get_type(JCC *vm, const char *name) {
    return ast_find_type(vm, name);
}

// Literal constructors (aliases for ast_node_* functions)
Node *ast_int_literal(JCC *vm, int64_t value) {
    return ast_node_num(vm, value);
}

Node *ast_string_literal(JCC *vm, const char *str) {
    return ast_node_string(vm, str);
}

Node *ast_var_ref(JCC *vm, const char *name) {
    return ast_node_ident(vm, name);
}

// Enum reflection wrappers (convenience functions with simplified signatures)
const char *ast_enum_name(Type *e) {
    return ast_type_name(e);
}

size_t ast_enum_value_count(Type *e) {
    int count = ast_enum_count(NULL, e);
    return count < 0 ? 0 : (size_t)count;
}

const char *ast_enum_value_name(Type *e, size_t index) {
    EnumConstant *ec = ast_enum_at(NULL, e, (int)index);
    return ast_enum_constant_name(ec);
}

int64_t ast_enum_value(Type *e, size_t index) {
    EnumConstant *ec = ast_enum_at(NULL, e, (int)index);
    return ast_enum_constant_value(ec);
}

// Control flow: switch statement
Node *ast_switch(JCC *vm, Node *condition) {
    if (!vm || !condition)
        return NULL;
    
    Node *node = alloc_node(vm, ND_SWITCH);
    node->cond = condition;
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
    case_node->begin = value->val;  // Assuming value is a numeric literal
    case_node->end = value->val;
    case_node->lhs = body;
    
    // Link to the switch's case chain
    if (switch_node->case_next == NULL) {
        switch_node->case_next = case_node;
    } else {
        Node *cur = switch_node->case_next;
        while (cur->case_next)
            cur = cur->case_next;
        cur->case_next = case_node;
    }
}

void ast_switch_set_default(JCC *vm, Node *switch_node, Node *body) {
    if (!vm || !switch_node || !body)
        return;
    
    if (switch_node->kind != ND_SWITCH)
        return;
    
    switch_node->default_case = body;
}

// Control flow: return statement
Node *ast_return(JCC *vm, Node *expr) {
    if (!vm)
        return NULL;
    
    Node *node = alloc_node(vm, ND_RETURN);
    node->lhs = expr;
    return node;
}

// Function construction
Node *ast_function(JCC *vm, const char *name, Type *return_type) {
    if (!vm || !name || !return_type)
        return NULL;
    
    // Create a function Obj
    Obj *func = calloc(1, sizeof(Obj));
    func->name = strdup(name);
    func->is_function = true;
    func->is_definition = true;
    func->ty = func_type(return_type);
    func->params = NULL;
    func->body = NULL;
    
    // Create a variable node pointing to the function
    Node *node = alloc_node(vm, ND_VAR);
    node->var = func;
    node->ty = func->ty;
    return node;
}

void ast_function_add_param(JCC *vm, Node *func_node, const char *name, Type *type) {
    if (!vm || !func_node || !name || !type)
        return;
    
    if (func_node->kind != ND_VAR || !func_node->var || !func_node->var->is_function)
        return;
    
    Obj *func = func_node->var;
    
    // Create parameter Obj
    Obj *param = calloc(1, sizeof(Obj));
    param->name = strdup(name);
    param->ty = type;
    param->is_local = true;
    
    // Add to params list
    if (func->params == NULL) {
        func->params = param;
    } else {
        Obj *cur = func->params;
        while (cur->next)
            cur = cur->next;
        cur->next = param;
    }
    
    // Also add to function type params
    Type *param_ty = calloc(1, sizeof(Type));
    *param_ty = *type;
    param_ty->next = NULL;
    
    if (func->ty->params == NULL) {
        func->ty->params = param_ty;
    } else {
        Type *cur = func->ty->params;
        while (cur->next)
            cur = cur->next;
        cur->next = param_ty;
    }
}

void ast_function_set_body(JCC *vm, Node *func_node, Node *body) {
    if (!vm || !func_node || !body)
        return;
    
    if (func_node->kind != ND_VAR || !func_node->var || !func_node->var->is_function)
        return;
    
    Obj *func = func_node->var;
    
    // Wrap body in a block if it isn't already
    if (body->kind != ND_BLOCK) {
        Node *block = alloc_node(vm, ND_BLOCK);
        block->body = body;
        body = block;
    }
    
    func->body = body;
}

// Struct construction
Node *ast_struct(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;

    Type *ty = struct_type();
    ty->kind = TY_STRUCT;

    // Create a synthetic token for the struct name
    // We use a minimal token with just the name string
    Token *name_tok = calloc(1, sizeof(Token));
    name_tok->kind = TK_IDENT;
    name_tok->loc = (char*)name;
    name_tok->len = strlen(name);
    ty->name = name_tok;

    // Initialize struct with no members yet
    ty->members = NULL;
    ty->align = 1;
    ty->size = 0;

    // Create a typedef-like node that represents this struct type
    // We return a node that can be used to reference the struct
    Node *node = alloc_node(vm, ND_NULL_EXPR);
    node->ty = ty;
    return node;
}

void ast_struct_add_field(JCC *vm, Node *struct_node, const char *name, Type *type) {
    if (!vm || !struct_node || !name || !type)
        return;

    Type *struct_ty = struct_node->ty;
    if (!struct_ty || (struct_ty->kind != TY_STRUCT && struct_ty->kind != TY_UNION))
        return;

    // Create a new member
    Member *mem = calloc(1, sizeof(Member));
    mem->ty = type;
    mem->align = type->align;

    // Create a synthetic token for the field name
    Token *name_tok = calloc(1, sizeof(Token));
    name_tok->kind = TK_IDENT;
    name_tok->loc = (char*)name;
    name_tok->len = strlen(name);
    mem->name = name_tok;
    mem->tok = name_tok;

    // Count existing members for idx
    int idx = 0;
    Member *last = NULL;
    for (Member *m = struct_ty->members; m; m = m->next) {
        idx++;
        last = m;
    }
    mem->idx = idx;

    // Add to member list
    if (last) {
        last->next = mem;
    } else {
        struct_ty->members = mem;
    }

    // Recalculate struct layout (simplified version - no bitfields)
    // Calculate offset for this new member
    int offset = 0;
    if (last) {
        // Start after the last member
        offset = last->offset + last->ty->size;
    }

    // Align the offset
    offset = (offset + type->align - 1) / type->align * type->align;
    mem->offset = offset;

    // Update struct size and alignment
    int new_size = offset + type->size;
    if (new_size > struct_ty->size)
        struct_ty->size = new_size;

    if (type->align > struct_ty->align)
        struct_ty->align = type->align;

    // Align final struct size to its alignment
    struct_ty->size = (struct_ty->size + struct_ty->align - 1) / struct_ty->align * struct_ty->align;
}