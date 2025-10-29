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

#include "internal.h"

Type *cc_find_type(JCC *vm, const char *name) {
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

bool cc_type_exists(JCC *vm, const char *name) {
    return cc_find_type(vm, name) != NULL;
}

int cc_enum_count(JCC *vm, Type *enum_type) {
    if (!enum_type || enum_type->kind != TY_ENUM) return -1;

    int count = 0;
    for (EnumConstant *ec = enum_type->enum_constants; ec; ec = ec->next)
        count++;
    return count;
}

EnumConstant *cc_enum_at(JCC *vm, Type *enum_type, int index) {
    if (!enum_type || enum_type->kind != TY_ENUM || index < 0)
        return NULL;

    EnumConstant *ec = enum_type->enum_constants;
    for (int i = 0; i < index && ec; i++)
        ec = ec->next;
    return ec;
}

EnumConstant *cc_enum_find(JCC *vm, Type *enum_type, const char *name) {
    if (!enum_type || enum_type->kind != TY_ENUM || !name)
        return NULL;

    for (EnumConstant *ec = enum_type->enum_constants; ec; ec = ec->next)
        if (strcmp(ec->name, name) == 0)
            return ec;
    return NULL;
}

const char *cc_enum_constant_name(EnumConstant *ec) {
    return ec ? ec->name : NULL;
}

int cc_enum_constant_value(EnumConstant *ec) {
    return ec ? ec->value : 0;
}

int cc_struct_member_count(JCC *vm, Type *struct_type) {
    if (!struct_type)
        return -1;
    if (struct_type->kind != TY_STRUCT && struct_type->kind != TY_UNION)
        return -1;

    int count = 0;
    for (Member *m = struct_type->members; m; m = m->next)
        count++;
    return count;
}

Member *cc_struct_member_at(JCC *vm, Type *struct_type, int index) {
    if (!struct_type || index < 0)
        return NULL;
    if (struct_type->kind != TY_STRUCT && struct_type->kind != TY_UNION)
        return NULL;

    Member *m = struct_type->members;
    for (int i = 0; i < index && m; i++)
        m = m->next;
    return m;
}

Member *cc_struct_member_find(JCC *vm, Type *struct_type, const char *name) {
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

const char *cc_member_name(Member *m) {
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

Type *cc_member_type(Member *m) {
    return m ? m->ty : NULL;
}

int cc_member_offset(Member *m) {
    return m ? m->offset : 0;
}

bool cc_member_is_bitfield(Member *m) {
    return m ? m->is_bitfield : false;
}

int cc_member_bitfield_width(Member *m) {
    return (m && m->is_bitfield) ? m->bit_width : 0;
}

TypeKind cc_type_kind(Type *ty) {
    return ty ? ty->kind : TY_VOID;
}

int cc_type_size(Type *ty) {
    return ty ? ty->size : 0;
}

int cc_type_align(Type *ty) {
    return ty ? ty->align : 0;
}

bool cc_type_is_unsigned(Type *ty) {
    return ty ? ty->is_unsigned : false;
}

bool cc_type_is_const(Type *ty) {
    return ty ? ty->is_const : false;
}

Type *cc_type_base(Type *ty) {
    if (!ty)
        return NULL;
    if (ty->kind != TY_PTR && ty->kind != TY_ARRAY && ty->kind != TY_VLA)
        return NULL;
    return ty->base;
}

int cc_type_array_len(Type *ty) {
    if (!ty || ty->kind != TY_ARRAY)
        return -1;
    return ty->array_len;
}

Type *cc_type_return_type(Type *ty) {
    if (!ty || ty->kind != TY_FUNC)
        return NULL;
    return ty->return_ty;
}

int cc_type_param_count(Type *ty) {
    if (!ty || ty->kind != TY_FUNC)
        return -1;

    int count = 0;
    for (Type *p = ty->params; p; p = p->next)
        count++;
    return count;
}

Type *cc_type_param_at(Type *ty, int index) {
    if (!ty || ty->kind != TY_FUNC || index < 0)
        return NULL;

    Type *p = ty->params;
    for (int i = 0; i < index && p; i++)
        p = p->next;
    return p;
}

bool cc_type_is_variadic(Type *ty) {
    if (!ty || ty->kind != TY_FUNC)
        return false;
    return ty->is_variadic;
}

const char *cc_type_name(Type *ty) {
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

bool cc_is_integer(Type *ty) {
    return is_integer(ty);
}

bool cc_is_flonum(Type *ty) {
    return is_flonum(ty);
}

bool cc_is_pointer(Type *ty) {
    return ty && (ty->kind == TY_PTR || ty->kind == TY_ARRAY || ty->kind == TY_VLA);
}

bool cc_is_array(Type *ty) {
    return ty && (ty->kind == TY_ARRAY || ty->kind == TY_VLA);
}

bool cc_is_function(Type *ty) {
    return ty && ty->kind == TY_FUNC;
}

bool cc_is_struct(Type *ty) {
    return ty && ty->kind == TY_STRUCT;
}

bool cc_is_union(Type *ty) {
    return ty && ty->kind == TY_UNION;
}

bool cc_is_enum(Type *ty) {
    return ty && ty->kind == TY_ENUM;
}

Obj *cc_find_global(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;

    for (Obj *obj = vm->globals; obj; obj = obj->next)
        if (strcmp(obj->name, name) == 0)
            return obj;
    return NULL;
}

int cc_global_count(JCC *vm) {
    if (!vm)
        return 0;

    int count = 0;
    for (Obj *obj = vm->globals; obj; obj = obj->next)
        count++;
    return count;
}

Obj *cc_global_at(JCC *vm, int index) {
    if (!vm || index < 0)
        return NULL;

    Obj *obj = vm->globals;
    for (int i = 0; i < index && obj; i++)
        obj = obj->next;
    return obj;
}

const char *cc_obj_name(Obj *obj) {
    return obj ? obj->name : NULL;
}

Type *cc_obj_type(Obj *obj) {
    return obj ? obj->ty : NULL;
}

bool cc_obj_is_function(Obj *obj) {
    return obj ? obj->is_function : false;
}

bool cc_obj_is_definition(Obj *obj) {
    return obj ? obj->is_definition : false;
}

bool cc_obj_is_static(Obj *obj) {
    return obj ? obj->is_static : false;
}

int cc_func_param_count(Obj *func) {
    if (!func || !func->is_function)
        return -1;

    int count = 0;
    for (Obj *p = func->params; p; p = p->next)
        count++;
    return count;
}

Obj *cc_func_param_at(Obj *func, int index) {
    if (!func || !func->is_function || index < 0)
        return NULL;

    Obj *p = func->params;
    for (int i = 0; i < index && p; i++)
        p = p->next;
    return p;
}

Node *cc_func_body(Obj *func) {
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

Node *cc_node_num(JCC *vm, long long value) {
    if (!vm)
        return NULL;

    Node *node = alloc_node(vm, ND_NUM);
    node->val = value;
    node->ty = ty_long;
    return node;
}

Node *cc_node_float(JCC *vm, double value) {
    if (!vm) return NULL;

    Node *node = alloc_node(vm, ND_NUM);
    node->fval = value;
    node->ty = ty_double;
    return node;
}

Node *cc_node_string(JCC *vm, const char *str) {
    if (!vm || !str)
        return NULL;

    // String literals are complex - they need to be added to the data segment
    // For now, we'll create a simplified version
    Node *node = alloc_node(vm, ND_NUM);
    // This is a placeholder - proper string literal creation would need more work
    return node;
}

Node *cc_node_ident(JCC *vm, const char *name) {
    if (!vm || !name)
        return NULL;

    // Look up the variable
    Obj *var = cc_find_global(vm, name);
    if (!var)
        return NULL;

    Node *node = alloc_node(vm, ND_VAR);
    node->var = var;
    node->ty = var->ty;
    return node;
}

Node *cc_node_binary(JCC *vm, NodeKind op, Node *left, Node *right) {
    if (!vm || !left || !right)
        return NULL;

    Node *node = alloc_node(vm, op);
    node->lhs = left;
    node->rhs = right;
    // Type inference would happen in add_type() pass
    return node;
}

Node *cc_node_unary(JCC *vm, NodeKind op, Node *operand) {
    if (!vm || !operand)
        return NULL;

    Node *node = alloc_node(vm, op);
    node->lhs = operand;
    return node;
}

Node *cc_node_call(JCC *vm, const char *func_name, Node **args, int arg_count) {
    if (!vm || !func_name)
        return NULL;

    Node *node = alloc_node(vm, ND_FUNCALL);

    // Create identifier node for function name
    Obj *func = cc_find_global(vm, func_name);
    if (!func || !func->is_function)
        return NULL;

    Node *fn = cc_node_ident(vm, func_name);
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

Node *cc_node_member(JCC *vm, Node *object, const char *member_name) {
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
    Member *mem = cc_struct_member_find(vm, ty, member_name);
    if (!mem)
        return NULL;

    Node *node = alloc_node(vm, ND_MEMBER);
    node->lhs = object;
    node->member = mem;
    node->ty = mem->ty;
    return node;
}

Node *cc_node_cast(JCC *vm, Node *expr, Type *target_type) {
    if (!vm || !expr || !target_type)
        return NULL;
    return new_cast(vm, expr, target_type);
}

Node *cc_node_block(JCC *vm, Node **stmts, int stmt_count) {
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

// ============================================================================
// AST Node Inspection
// ============================================================================

NodeKind cc_node_kind(Node *node) {
    return node ? node->kind : ND_NULL_EXPR;
}

Type *cc_node_type(Node *node) {
    return node ? node->ty : NULL;
}

long long cc_node_int_value(Node *node) {
    if (!node || node->kind != ND_NUM)
        return 0;
    return node->val;
}

double cc_node_float_value(Node *node) {
    if (!node || node->kind != ND_NUM)
        return 0.0;
    return node->fval;
}

const char *cc_node_string_value(Node *node) {
    // This would need to extract string from a string literal node
    // Placeholder for now
    return NULL;
}

Node *cc_node_left(Node *node) {
    return node ? node->lhs : NULL;
}

Node *cc_node_right(Node *node) {
    return node ? node->rhs : NULL;
}

const char *cc_node_func_name(Node *node) {
    if (!node || node->kind != ND_FUNCALL)
        return NULL;
    if (!node->lhs || node->lhs->kind != ND_VAR)
        return NULL;
    return cc_obj_name(node->lhs->var);
}

int cc_node_arg_count(Node *node) {
    if (!node || node->kind != ND_FUNCALL)
        return 0;

    int count = 0;
    for (Node *arg = node->args; arg; arg = arg->next)
        count++;
    return count;
}

Node *cc_node_arg_at(Node *node, int index) {
    if (!node || node->kind != ND_FUNCALL || index < 0)
        return NULL;

    Node *arg = node->args;
    for (int i = 0; i < index && arg; i++)
        arg = arg->next;
    return arg;
}

int cc_node_stmt_count(Node *node) {
    if (!node || node->kind != ND_BLOCK)
        return 0;

    int count = 0;
    for (Node *stmt = node->body; stmt; stmt = stmt->next)
        count++;
    return count;
}

Node *cc_node_stmt_at(Node *node, int index) {
    if (!node || node->kind != ND_BLOCK || index < 0)
        return NULL;

    Node *stmt = node->body;
    for (int i = 0; i < index && stmt; i++)
        stmt = stmt->next;
    return stmt;
}

Obj *cc_node_var(Node *node) {
    if (!node || node->kind != ND_VAR)
        return NULL;
    return node->var;
}

Token *cc_node_token(Node *node) {
    return node ? node->tok : NULL;
}

const char *cc_token_filename(Token *tok) {
    if (!tok)
        return NULL;
    return tok->filename ? tok->filename : (tok->file ? tok->file->name : NULL);
}

int cc_token_line(Token *tok) {
    return tok ? tok->line_no : 0;
}

int cc_token_column(Token *tok) {
    // Column calculation would require tracking position in line
    // For now, return 0 TODO: implement column tracking 
    return 0;
}

int cc_token_text(Token *tok, char *buffer, int bufsize) {
    if (!tok || !buffer || bufsize <= 0)
        return 0;

    int len = tok->len;
    if (len >= bufsize)
        len = bufsize - 1;

    strncpy(buffer, tok->loc, len);
    buffer[len] = '\0';
    return len;
}

// ============================================================================
// Type Construction
// ============================================================================

Type *cc_make_pointer(Type *base) {
    if (!base)
        return NULL;
    return pointer_to(base);
}

Type *cc_make_array(Type *base, int length) {
    if (!base || length < 0)
        return NULL;
    return array_of(base, length);
}

Type *cc_make_function(Type *return_type, Type **param_types, int param_count, bool is_variadic) {
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
