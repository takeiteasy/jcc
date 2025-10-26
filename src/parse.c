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

 This file was original part of chibicc by Rui Ueyama (MIT) https://github.com/rui314/chibicc
*/

// This file contains a recursive descent parser for C.
//
// Most functions in this file are named after the symbols they are
// supposed to read from an input token list. For example, stmt() is
// responsible for reading a statement from a token list. The function
// then construct an AST node representing a statement.
//
// Each function conceptually returns two values, an AST node and
// remaining part of the input tokens. Since C doesn't support
// multiple return values, the remaining tokens are returned to the
// caller via a pointer argument.
//
// Input tokens are represented by a linked list. Unlike many recursive
// descent parsers, we don't have the notion of the "input token stream".
// Most parsing functions don't change the global state of the parser.
// So it is very easy to lookahead arbitrary number of tokens in this
// parser.

#include "jcc.h"
#include "./internal.h"

#ifndef MAX
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#endif
#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

// Scope for local variables, global variables, typedefs
// or enum constants
typedef struct {
    Obj *var;
    Type *type_def;
    Type *enum_ty;
    int enum_val;
} VarScope;

// Variable attributes such as typedef or extern.
typedef struct {
    bool is_typedef;
    bool is_static;
    bool is_extern;
    bool is_inline;
    bool is_tls;
    bool is_constexpr;
    int align;
} VarAttr;

// This struct represents a variable initializer. Since initializers
// can be nested (e.g. `int x[2][2] = {{1, 2}, {3, 4}}`), this struct
// is a tree data structure.
typedef struct Initializer Initializer;
struct Initializer {
    Initializer *next;
    Type *ty;
    Token *tok;
    bool is_flexible;

    // If it's not an aggregate type and has an initializer,
    // `expr` has an initialization expression.
    Node *expr;

    // If it's an initializer for an aggregate type (e.g. array or struct),
    // `children` has initializers for its children.
    Initializer **children;

    // Only one member can be initialized for a union.
    // `mem` is used to clarify which member is initialized.
    Member *mem;
};

// For local variable initializer.
typedef struct InitDesg InitDesg;
struct InitDesg {
    InitDesg *next;
    int idx;
    Member *member;
    Obj *var;
};

static bool is_typename(JCC *vm, Token *tok);
static Type *declspec(JCC *vm, Token **rest, Token *tok, VarAttr *attr);
static Type *typename(JCC *vm, Token **rest, Token *tok);
static Type *enum_specifier(JCC *vm, Token **rest, Token *tok);
static Type *typeof_specifier(JCC *vm, Token **rest, Token *tok);
static Type *type_suffix(JCC *vm, Token **rest, Token *tok, Type *ty);
static Type *declarator(JCC *vm, Token **rest, Token *tok, Type *ty);
static Token *attribute_list(JCC *vm, Token *tok, Type *ty);
static Node *declaration(JCC *vm, Token **rest, Token *tok, Type *basety, VarAttr *attr);
static void array_initializer2(JCC *vm, Token **rest, Token *tok, Initializer *init, int i);
static void struct_initializer2(JCC *vm, Token **rest, Token *tok, Initializer *init, Member *mem);
static void initializer2(JCC *vm, Token **rest, Token *tok, Initializer *init);
static Initializer *initializer(JCC *vm, Token **rest, Token *tok, Type *ty, Type **new_ty);
static Node *lvar_initializer(JCC *vm, Token **rest, Token *tok, Obj *var);
static void gvar_initializer(JCC *vm, Token **rest, Token *tok, Obj *var);
static Node *compound_stmt(JCC *vm, Token **rest, Token *tok);
static Node *stmt(JCC *vm, Token **rest, Token *tok);
static Node *expr_stmt(JCC *vm, Token **rest, Token *tok);
static Node *expr(JCC *vm, Token **rest, Token *tok);
static int64_t eval(JCC *vm, Node *node);
static int64_t eval2(JCC *vm, Node *node, char ***label);
static int64_t eval_rval(JCC *vm, Node *node, char ***label);
static bool is_const_expr(JCC *vm, Node *node);
static Node *assign(JCC *vm, Token **rest, Token *tok);
static Node *logor(JCC *vm, Token **rest, Token *tok);
static double eval_double(JCC *vm, Node *node);
static Node *conditional(JCC *vm, Token **rest, Token *tok);
static Node *logand(JCC *vm, Token **rest, Token *tok);
static Node *bitor(JCC *vm, Token **rest, Token *tok);
static Node *bitxor(JCC *vm, Token **rest, Token *tok);
static Node *bitand(JCC *vm, Token **rest, Token *tok);
static Node *equality(JCC *vm, Token **rest, Token *tok);
static Node *relational(JCC *vm, Token **rest, Token *tok);
static Node *shift(JCC *vm, Token **rest, Token *tok);
static Node *add(JCC *vm, Token **rest, Token *tok);
static Node *new_add(JCC *vm, Node *lhs, Node *rhs, Token *tok);
static Node *new_sub(JCC *vm, Node *lhs, Node *rhs, Token *tok);
static Node *mul(JCC *vm, Token **rest, Token *tok);
static Node *cast(JCC *vm, Token **rest, Token *tok);
static Member *get_struct_member(Type *ty, Token *tok);
static Type *struct_decl(JCC *vm, Token **rest, Token *tok);
static Type *union_decl(JCC *vm, Token **rest, Token *tok);
static Node *postfix(JCC *vm, Token **rest, Token *tok);
static Node *funcall(JCC *vm, Token **rest, Token *tok, Node *node);
static Node *unary(JCC *vm, Token **rest, Token *tok);
static Node *primary(JCC *vm, Token **rest, Token *tok);
static Token *parse_typedef(JCC *vm, Token *tok, Type *basety);
static bool is_function(JCC *vm, Token *tok);
static Token *function(JCC *vm, Token *tok, Type *basety, VarAttr *attr);
static Token *global_variable(JCC *vm, Token *tok, Type *basety, VarAttr *attr);

static int align_to(int n, int align) {
    return (n + align - 1) / align * align;
}

static int align_down(int n, int align) {
    return align_to(n - align + 1, align);
}

static void enter_scope(JCC *vm) {
    Scope *sc = calloc(1, sizeof(Scope));
    sc->next = vm->scope;
    vm->scope = sc;
}

static void leave_scope(JCC *vm) {
    vm->scope = vm->scope->next;
}

// Find a variable by name.
static VarScope *find_var(JCC *vm, Token *tok) {
    for (Scope *sc = vm->scope; sc; sc = sc->next) {
        VarScope *sc2 = hashmap_get2(&sc->vars, tok->loc, tok->len);
        if (sc2)
            return sc2;
    }
    return NULL;
}

static Type *find_tag(JCC *vm, Token *tok) {
    for (Scope *sc = vm->scope; sc; sc = sc->next) {
        Type *ty = hashmap_get2(&sc->tags, tok->loc, tok->len);
        if (ty)
            return ty;
    }
    return NULL;
}

static Node *new_node(NodeKind kind, Token *tok) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

static Node *new_num(int64_t val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

static Node *new_long(int64_t val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    node->ty = ty_long;
    return node;
}

static Node *new_ulong(long val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    node->ty = ty_ulong;
    return node;
}

static Node *new_var_node(Obj *var, Token *tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

static Node *new_vla_ptr(Obj *var, Token *tok) {
    Node *node = new_node(ND_VLA_PTR, tok);
    node->var = var;
    return node;
}

Node *new_cast(JCC *vm, Node *expr, Type *ty) {
    add_type(vm, expr);
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_CAST;
    node->tok = expr->tok;
    node->lhs = expr;
    node->ty = copy_type(ty);
    return node;
}

static VarScope *push_scope(JCC *vm, char *name) {
    VarScope *sc = calloc(1, sizeof(VarScope));
    hashmap_put(&vm->scope->vars, name, sc);
    return sc;
}

static Initializer *new_initializer(Type *ty, bool is_flexible) {
    Initializer *init = calloc(1, sizeof(Initializer));
    init->ty = ty;

    if (ty->kind == TY_ARRAY) {
        if (is_flexible && ty->size < 0) {
            init->is_flexible = true;
            return init;
        }

        init->children = calloc(ty->array_len, sizeof(Initializer *));
        for (int i = 0; i < ty->array_len; i++)
            init->children[i] = new_initializer(ty->base, false);
        return init;
    }

    if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
        // Count the number of struct members.
        int len = 0;
        for (Member *mem = ty->members; mem; mem = mem->next)
            len++;

        init->children = calloc(len, sizeof(Initializer *));

        for (Member *mem = ty->members; mem; mem = mem->next) {
            if (is_flexible && ty->is_flexible && !mem->next) {
                Initializer *child = calloc(1, sizeof(Initializer));
                child->ty = mem->ty;
                child->is_flexible = true;
                init->children[mem->idx] = child;
            } else {
                init->children[mem->idx] = new_initializer(mem->ty, false);
            }
        }
        return init;
    }

    return init;
}

static Obj *new_var(JCC *vm, char *name, Type *ty) {
    Obj *var = calloc(1, sizeof(Obj));
    var->name = name;
    var->ty = ty;
    var->align = ty->align;
    push_scope(vm, name)->var = var;
    return var;
}

static Obj *new_lvar(JCC *vm, char *name, Type *ty) {
    Obj *var = new_var(vm, name, ty);
    var->is_local = true;
    var->next = vm->locals;
    vm->locals = var;
    return var;
}

static Obj *new_gvar(JCC *vm, char *name, Type *ty) {
    Obj *var = new_var(vm, name, ty);
    var->next = vm->globals;
    var->is_static = true;
    var->is_definition = true;
    vm->globals = var;
    return var;
}

static char *new_unique_name(void) {
    static int id = 0;
    return format(".L..%d", id++);
}

static Obj *new_anon_gvar(JCC *vm, Type *ty) {
    return new_gvar(vm, new_unique_name(), ty);
}

static Obj *new_string_literal(JCC *vm, char *p, Type *ty) {
    Obj *var = new_anon_gvar(vm, ty);
    var->init_data = p;
    return var;
}

static char *get_ident(JCC *vm, Token *tok) {
    if (tok->kind != TK_IDENT)
        error_tok(vm, tok, "expected an identifier");
    return strndup(tok->loc, tok->len);
}

static Type *find_typedef(JCC *vm, Token *tok) {
    if (tok->kind == TK_IDENT) {
        VarScope *sc = find_var(vm, tok);
        if (sc)
            return sc->type_def;
    }
    return NULL;
}

static void push_tag_scope(JCC *vm, Token *tok, Type *ty) {
    hashmap_put2(&vm->scope->tags, tok->loc, tok->len, ty);
}

// declspec = ("void" | "_Bool" | "char" | "short" | "int" | "long"
//             | "typedef" | "static" | "extern" | "inline"
//             | "_Thread_local" | "__thread"
//             | "signed" | "unsigned"
//             | struct-decl | union-decl | typedef-name
//             | enum-specifier | typeof-specifier
//             | "const" | "volatile" | "auto" | "register" | "restrict"
//             | "__restrict" | "__restrict__" | "_Noreturn")+
//
// The order of typenames in a type-specifier doesn't matter. For
// example, `int long static` means the same as `static long int`.
// That can also be written as `static long` because you can omit
// `int` if `long` or `short` are specified. However, something like
// `char int` is not a valid type specifier. We have to accept only a
// limited combinations of the typenames.
//
// In this function, we count the number of occurrences of each typename
// while keeping the "current" type object that the typenames up
// until that point represent. When we reach a non-typename token,
// we returns the current type object.
static Type *declspec(JCC *vm, Token **rest, Token *tok, VarAttr *attr) {
    // We use a single integer as counters for all typenames.
    // For example, bits 0 and 1 represents how many times we saw the
    // keyword "void" so far. With this, we can use a switch statement
    // as you can see below.
    enum {
        VOID     = 1 << 0,
        BOOL     = 1 << 2,
        CHAR     = 1 << 4,
        SHORT    = 1 << 6,
        INT      = 1 << 8,
        LONG     = 1 << 10,
        FLOAT    = 1 << 12,
        DOUBLE   = 1 << 14,
        OTHER    = 1 << 16,
        SIGNED   = 1 << 17,
        UNSIGNED = 1 << 18,
    };

    Type *ty = ty_int;
    int counter = 0;
    bool is_atomic = false;
    bool is_const = false;

    while (is_typename(vm, tok)) {
        // Handle __attribute__ at the beginning of declspec
        if (equal(tok, "__attribute__")) {
            tok = attribute_list(vm, tok, NULL);
            continue;
        }

        // Handle storage class specifiers.
        if (equal(tok, "typedef") || equal(tok, "static") || equal(tok, "extern") ||
            equal(tok, "inline") || equal(tok, "_Thread_local") || equal(tok, "__thread") ||
            equal(tok, "constexpr")) {
            if (!attr)
                error_tok(vm, tok, "storage class specifier is not allowed in this context");

            if (equal(tok, "typedef"))
                attr->is_typedef = true;
            else if (equal(tok, "static"))
                attr->is_static = true;
            else if (equal(tok, "extern"))
                attr->is_extern = true;
            else if (equal(tok, "inline"))
                attr->is_inline = true;
            else if (equal(tok, "constexpr"))
                attr->is_constexpr = true;
            else
                attr->is_tls = true;

            if (attr->is_typedef &&
                attr->is_static + attr->is_extern + attr->is_inline + attr->is_tls > 1)
                error_tok(vm, tok, "typedef may not be used together with static,"
                          " extern, inline, __thread or _Thread_local");
            tok = tok->next;
            continue;
        }

        // Handle const qualifier (now enforced)
        if (consume(vm, &tok, tok, "const")) {
            is_const = true;
            continue;
        }

        // These keywords are recognized but ignored.
        if (consume(vm, &tok, tok, "volatile") ||
            consume(vm, &tok, tok, "auto") || consume(vm, &tok, tok, "register") ||
            consume(vm, &tok, tok, "restrict") || consume(vm, &tok, tok, "__restrict") ||
            consume(vm, &tok, tok, "__restrict__") || consume(vm, &tok, tok, "_Noreturn"))
            continue;

        if (equal(tok, "_Atomic")) {
            tok = tok->next;
            if (equal(tok , "(")) {
                ty = typename(vm, &tok, tok->next);
                tok = skip(vm, tok, ")");
            }
            is_atomic = true;
            continue;
        }

        if (equal(tok, "_Alignas")) {
            if (!attr)
                error_tok(vm, tok, "_Alignas is not allowed in this context");
            tok = skip(vm, tok->next, "(");

            if (is_typename(vm, tok))
                attr->align = typename(vm, &tok, tok)->align;
            else
                attr->align = const_expr(vm, &tok, tok);
            tok = skip(vm, tok, ")");
            continue;
        }

        // Handle user-defined types.
        Type *ty2 = find_typedef(vm, tok);
        if (equal(tok, "struct") || equal(tok, "union") || equal(tok, "enum") ||
            equal(tok, "typeof") || ty2) {
            if (counter)
                break;

            if (equal(tok, "struct")) {
                ty = struct_decl(vm, &tok, tok->next);
            } else if (equal(tok, "union")) {
                ty = union_decl(vm, &tok, tok->next);
            } else if (equal(tok, "enum")) {
                ty = enum_specifier(vm, &tok, tok->next);
            } else if (equal(tok, "typeof")) {
                ty = typeof_specifier(vm, &tok, tok->next);
            } else {
                ty = ty2;
                tok = tok->next;
            }

            counter += OTHER;
            continue;
        }

        // Handle built-in types.
        if (equal(tok, "void"))
            counter += VOID;
        else if (equal(tok, "_Bool"))
            counter += BOOL;
        else if (equal(tok, "char"))
            counter += CHAR;
        else if (equal(tok, "short"))
            counter += SHORT;
        else if (equal(tok, "int"))
            counter += INT;
        else if (equal(tok, "long"))
            counter += LONG;
        else if (equal(tok, "float"))
            counter += FLOAT;
        else if (equal(tok, "double"))
            counter += DOUBLE;
        else if (equal(tok, "signed"))
            counter |= SIGNED;
        else if (equal(tok, "unsigned"))
            counter |= UNSIGNED;
        else
            unreachable();

        switch (counter) {
            case VOID:
                ty = ty_void;
                break;
            case BOOL:
                ty = ty_bool;
                break;
            case CHAR:
            case SIGNED + CHAR:
                ty = ty_char;
                break;
            case UNSIGNED + CHAR:
                ty = ty_uchar;
                break;
            case SHORT:
            case SHORT + INT:
            case SIGNED + SHORT:
            case SIGNED + SHORT + INT:
                ty = ty_short;
                break;
            case UNSIGNED + SHORT:
            case UNSIGNED + SHORT + INT:
                ty = ty_ushort;
                break;
            case INT:
            case SIGNED:
            case SIGNED + INT:
                ty = ty_int;
                break;
            case UNSIGNED:
            case UNSIGNED + INT:
                ty = ty_uint;
                break;
            case LONG:
            case LONG + INT:
            case LONG + LONG:
            case LONG + LONG + INT:
            case SIGNED + LONG:
            case SIGNED + LONG + INT:
            case SIGNED + LONG + LONG:
            case SIGNED + LONG + LONG + INT:
                ty = ty_long;
                break;
            case UNSIGNED + LONG:
            case UNSIGNED + LONG + INT:
            case UNSIGNED + LONG + LONG:
            case UNSIGNED + LONG + LONG + INT:
                ty = ty_ulong;
                break;
            case FLOAT:
                ty = ty_float;
                break;
            case DOUBLE:
                ty = ty_double;
                break;
            case LONG + DOUBLE:
                ty = ty_ldouble;
                break;
            default:
                error_tok(vm, tok, "invalid type");
        }

        tok = tok->next;
    }

    if (is_atomic) {
        ty = copy_type(ty);
        ty->is_atomic = true;
    }

    if (is_const) {
        ty = copy_type(ty);
        ty->is_const = true;
    }

    *rest = tok;
    return ty;
}

// func-params = ("void" | param ("," param)* ("," "...")?)? ")"
// param       = declspec declarator
static Type *func_params(JCC *vm, Token **rest, Token *tok, Type *ty) {
    if (equal(tok, "void") && equal(tok->next, ")")) {
        *rest = tok->next->next;
        return func_type(ty);
    }

    Type head = {};
    Type *cur = &head;
    bool is_variadic = false;

    while (!equal(tok, ")")) {
        if (cur != &head)
            tok = skip(vm, tok, ",");

        if (equal(tok, "...")) {
            is_variadic = true;
            tok = tok->next;
            skip(vm, tok, ")");
            break;
        }

        Type *ty2 = declspec(vm, &tok, tok, NULL);
        ty2 = declarator(vm, &tok, tok, ty2);

        Token *name = ty2->name;

        if (ty2->kind == TY_ARRAY) {
            // "array of T" is converted to "pointer to T" only in the parameter
            // context. For example, *argv[] is converted to **argv by this.
            ty2 = pointer_to(ty2->base);
            ty2->name = name;
        } else if (ty2->kind == TY_FUNC) {
            // Likewise, a function is converted to a pointer to a function
            // only in the parameter context.
            ty2 = pointer_to(ty2);
            ty2->name = name;
        }

        cur = cur->next = copy_type(ty2);
    }

    if (cur == &head)
        is_variadic = true;

    ty = func_type(ty);
    ty->params = head.next;
    ty->is_variadic = is_variadic;
    *rest = tok->next;
    return ty;
}

// array-dimensions = ("static" | "restrict")* const-expr? "]" type-suffix
static Type *array_dimensions(JCC *vm, Token **rest, Token *tok, Type *ty) {
    while (equal(tok, "static") || equal(tok, "restrict"))
        tok = tok->next;

    if (equal(tok, "]")) {
        ty = type_suffix(vm, rest, tok->next, ty);
        return array_of(ty, -1);
    }

    Node *expr = conditional(vm, &tok, tok);
    tok = skip(vm, tok, "]");
    ty = type_suffix(vm, rest, tok, ty);

    if (ty->kind == TY_VLA || !is_const_expr(vm, expr))
        return vla_of(ty, expr);
    return array_of(ty, eval(vm, expr));
}

// type-suffix = "(" func-params
//             | "[" array-dimensions
//             | ε
static Type *type_suffix(JCC *vm, Token **rest, Token *tok, Type *ty) {
    if (equal(tok, "("))
        return func_params(vm, rest, tok->next, ty);

    if (equal(tok, "["))
        return array_dimensions(vm, rest, tok->next, ty);

    *rest = tok;
    return ty;
}

// pointers = ("*" ("const" | "volatile" | "restrict")*)*
static Type *pointers(JCC *vm, Token **rest, Token *tok, Type *ty) {
    while (consume(vm, &tok, tok, "*")) {
        ty = pointer_to(ty);
        // Handle const qualification on the pointer itself
        // Example: "int *const p" makes the pointer const, not the pointee
        while (equal(tok, "const") || equal(tok, "volatile") || equal(tok, "restrict") ||
               equal(tok, "__restrict") || equal(tok, "__restrict__")) {
            if (equal(tok, "const")) {
                ty = copy_type(ty);
                ty->is_const = true;
            }
            tok = tok->next;
        }
    }
    *rest = tok;
    return ty;
}

// declarator = attribute? pointers ("(" ident ")" | "(" declarator ")" | ident) type-suffix attribute?
static Type *declarator(JCC *vm, Token **rest, Token *tok, Type *ty) {
    // Handle __attribute__ before declarator
    tok = attribute_list(vm, tok, ty);
    
    ty = pointers(vm, &tok, tok, ty);

    if (equal(tok, "(")) {
        Token *start = tok;
        Type dummy = {};
        declarator(vm, &tok, start->next, &dummy);
        tok = skip(vm, tok, ")");
        ty = type_suffix(vm, &tok, tok, ty);
        ty = declarator(vm, &tok, start->next, ty);
        
        // Handle __attribute__ after declarator
        tok = attribute_list(vm, tok, ty);
        *rest = tok;
        return ty;
    }

    Token *name = NULL;
    Token *name_pos = tok;

    if (tok->kind == TK_IDENT) {
        name = tok;
        tok = tok->next;
    }

    ty = type_suffix(vm, &tok, tok, ty);
    
    // Handle __attribute__ after declarator
    tok = attribute_list(vm, tok, ty);
    
    ty->name = name;
    ty->name_pos = name_pos;
    *rest = tok;
    return ty;
}

// abstract-declarator = attribute? pointers ("(" abstract-declarator ")")? type-suffix attribute?
static Type *abstract_declarator(JCC *vm, Token **rest, Token *tok, Type *ty) {
    // Handle __attribute__ before abstract declarator
    tok = attribute_list(vm, tok, ty);
    
    ty = pointers(vm, &tok, tok, ty);

    if (equal(tok, "(")) {
        Token *start = tok;
        Type dummy = {};
        abstract_declarator(vm, &tok, start->next, &dummy);
        tok = skip(vm, tok, ")");
        ty = type_suffix(vm, &tok, tok, ty);
        ty = abstract_declarator(vm, &tok, start->next, ty);
        
        // Handle __attribute__ after abstract declarator
        tok = attribute_list(vm, tok, ty);
        *rest = tok;
        return ty;
    }

    ty = type_suffix(vm, &tok, tok, ty);
    
    // Handle __attribute__ after abstract declarator
    tok = attribute_list(vm, tok, ty);
    *rest = tok;
    
    return ty;
}

// type-name = declspec abstract-declarator
static Type *typename(JCC *vm, Token **rest, Token *tok) {
    Type *ty = declspec(vm, &tok, tok, NULL);
    return abstract_declarator(vm, rest, tok, ty);
}

static bool is_end(Token *tok) {
    return equal(tok, "}") || (equal(tok, ",") && equal(tok->next, "}"));
}

static bool consume_end(Token **rest, Token *tok) {
    if (equal(tok, "}")) {
        *rest = tok->next;
        return true;
    }

    if (equal(tok, ",") && equal(tok->next, "}")) {
        *rest = tok->next->next;
        return true;
    }

    return false;
}

// enum-specifier = ident? "{" enum-list? "}"
//                | ident ("{" enum-list? "}")?
//
// enum-list      = ident ("=" num)? ("," ident ("=" num)?)* ","?
static Type *enum_specifier(JCC *vm, Token **rest, Token *tok) {
    Type *ty = enum_type();

    // Read a struct tag.
    Token *tag = NULL;
    if (tok->kind == TK_IDENT) {
        tag = tok;
        tok = tok->next;
    }

    if (tag && !equal(tok, "{")) {
        Type *ty = find_tag(vm, tag);
        if (!ty)
            error_tok(vm, tag, "unknown enum type");
        if (ty->kind != TY_ENUM)
            error_tok(vm, tag, "not an enum tag");
        *rest = tok;
        return ty;
    }

    tok = skip(vm, tok, "{");

    // Read an enum-list.
    int i = 0;
    int val = 0;
    struct EnumConstant *enum_tail = NULL;
    while (!consume_end(rest, tok)) {
        if (i++ > 0)
            tok = skip(vm, tok, ",");

        char *name = get_ident(vm, tok);
        tok = tok->next;

        if (equal(tok, "="))
            val = const_expr(vm, &tok, tok->next);

        VarScope *sc = push_scope(vm, name);
        sc->enum_ty = ty;
        sc->enum_val = val;
        
        // Store enum constant in Type structure for code emission
        struct EnumConstant *ec = calloc(1, sizeof(struct EnumConstant));
        ec->name = strdup(name);
        ec->value = val;
        ec->next = NULL;
        
        if (enum_tail) {
            enum_tail->next = ec;
        } else {
            ty->enum_constants = ec;
        }
        enum_tail = ec;
        
        val++;
    }

    if (tag)
        push_tag_scope(vm, tag, ty);
    return ty;
}

// typeof-specifier = "(" (expr | typename) ")"
static Type *typeof_specifier(JCC *vm, Token **rest, Token *tok) {
    tok = skip(vm, tok, "(");

    Type *ty;
    if (is_typename(vm, tok)) {
        ty = typename(vm, &tok, tok);
    } else {
        Node *node = expr(vm, &tok, tok);
        add_type(vm, node);
        ty = node->ty;
    }
    *rest = skip(vm, tok, ")");
    return ty;
}

// Get size for a type (no adjustment needed - types are already correct)
static int get_vm_size(Type *ty) {
    return ty->size;
}

// Generate code for computing a VLA size.
static Node *compute_vla_size(JCC *vm, Type *ty, Token *tok) {
    Node *node = new_node(ND_NULL_EXPR, tok);
    if (ty->base)
        node = new_binary(ND_COMMA, node, compute_vla_size(vm, ty->base, tok), tok);

    if (ty->kind != TY_VLA)
        return node;

    Node *base_sz;
    if (ty->base->kind == TY_VLA)
        base_sz = new_var_node(ty->base->vla_size, tok);
    else
        base_sz = new_num(get_vm_size(ty->base), tok);  // Use VM-adjusted size

    ty->vla_size = new_lvar(vm, "", ty_ulong);
    Node *expr = new_binary(ND_ASSIGN, new_var_node(ty->vla_size, tok),
                            new_binary(ND_MUL, ty->vla_len, base_sz, tok),
                            tok);
    return new_binary(ND_COMMA, node, expr, tok);
}

static Node *new_alloca(JCC *vm, Node *sz) {
    Node *node = new_unary(ND_FUNCALL, new_var_node(vm->builtin_alloca, sz->tok), sz->tok);
    node->func_ty = vm->builtin_alloca->ty;
    node->ty = vm->builtin_alloca->ty->return_ty;
    node->args = sz;
    add_type(vm, sz);
    return node;
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(JCC *vm, Token **rest, Token *tok, Type *basety, VarAttr *attr) {
    Node head = {};
    Node *cur = &head;
    int i = 0;

    while (!equal(tok, ";")) {
        if (i++ > 0)
            tok = skip(vm, tok, ",");

        Type *ty = declarator(vm, &tok, tok, basety);
        if (ty->kind == TY_VOID)
            error_tok(vm, tok, "variable declared void");
        if (!ty->name)
            error_tok(vm, ty->name_pos, "variable name omitted");

        if (attr && attr->is_static) {
            // static local variable
            Obj *var = new_anon_gvar(vm, ty);
            push_scope(vm, get_ident(vm, ty->name))->var = var;
            if (equal(tok, "="))
                gvar_initializer(vm, &tok, tok->next, var);
            continue;
        }

        // Generate code for computing a VLA size. We need to do this
        // even if ty is not VLA because ty may be a pointer to VLA
        // (e.g. int (*foo)[n][m] where n and m are variables.)
        cur = cur->next = new_unary(ND_EXPR_STMT, compute_vla_size(vm, ty, tok), tok);

        if (ty->kind == TY_VLA) {
            if (equal(tok, "="))
                error_tok(vm, tok, "variable-sized object may not be initialized");

            // Variable length arrays (VLAs) are translated to alloca() calls.
            // For example, `int x[n+2]` is translated to `tmp = n + 2,
            // x = alloca(tmp)`.
            Obj *var = new_lvar(vm, get_ident(vm, ty->name), ty);
            Token *tok = ty->name;
            Node *expr = new_binary(ND_ASSIGN, new_vla_ptr(var, tok),
                                    new_alloca(vm, new_var_node(ty->vla_size, tok)),
                                    tok);

            cur = cur->next = new_unary(ND_EXPR_STMT, expr, tok);
            continue;
        }

        Obj *var = new_lvar(vm, get_ident(vm, ty->name), ty);
        if (attr && attr->align)
            var->align = attr->align;

        if (equal(tok, "=")) {
            // Mark this variable as being initialized (allows const initialization)
            // NOTE: Don't clear this until after add_type is called on the function body
            // For now, just set it and it will be cleared when next variable is initialized
            // This works because initializations happen sequentially
            vm->initializing_var = var;
            Node *expr = lvar_initializer(vm, &tok, tok->next, var);
            cur = cur->next = new_unary(ND_EXPR_STMT, expr, tok);
            // Don't clear here - will be cleared by next init or at end of parsing
        }

        if (var->ty->size < 0)
            error_tok(vm, ty->name, "variable has incomplete type");
        if (var->ty->kind == TY_VOID)
            error_tok(vm, ty->name, "variable declared void");
    }

    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

static Token *skip_excess_element(JCC *vm, Token *tok) {
    if (equal(tok, "{")) {
        tok = skip_excess_element(vm, tok->next);
        return skip(vm, tok, "}");
    }

    assign(vm, &tok, tok);
    return tok;
}

// string-initializer = string-literal
static void string_initializer(Token **rest, Token *tok, Initializer *init) {
    if (init->is_flexible)
        *init = *new_initializer(array_of(init->ty->base, tok->ty->array_len), false);

    int len = MIN(init->ty->array_len, tok->ty->array_len);

    switch (init->ty->base->size) {
        case 1: {
            char *str = tok->str;
            for (int i = 0; i < len; i++)
                init->children[i]->expr = new_num(str[i], tok);
            break;
        }
        case 2: {
            uint16_t *str = (uint16_t *)tok->str;
            for (int i = 0; i < len; i++)
                init->children[i]->expr = new_num(str[i], tok);
            break;
        }
        case 4: {
            uint32_t *str = (uint32_t *)tok->str;
            for (int i = 0; i < len; i++)
                init->children[i]->expr = new_num(str[i], tok);
            break;
        }
        default:
            unreachable();
    }

    *rest = tok->next;
}

// array-designator = "[" const-expr "]"
//
// C99 added the designated initializer to the language, which allows
// programmers to move the "cursor" of an initializer to any element.
// The syntax looks like this:
//
//   int x[10] = { 1, 2, [5]=3, 4, 5, 6, 7 };
//
// `[5]` moves the cursor to the 5th element, so the 5th element of x
// is set to 3. Initialization then continues forward in order, so
// 6th, 7th, 8th and 9th elements are initialized with 4, 5, 6 and 7,
// respectively. Unspecified elements (in this case, 3rd and 4th
// elements) are initialized with zero.
//
// Nesting is allowed, so the following initializer is valid:
//
//   int x[5][10] = { [5][8]=1, 2, 3 };
//
// It sets x[5][8], x[5][9] and x[6][0] to 1, 2 and 3, respectively.
//
// Use `.fieldname` to move the cursor for a struct initializer. E.g.
//
//   struct { int a, b, c; } x = { .c=5 };
//
// The above initializer sets x.c to 5.
static void array_designator(JCC *vm, Token **rest, Token *tok, Type *ty, int *begin, int *end) {
    *begin = const_expr(vm, &tok, tok->next);
    if (*begin >= ty->array_len)
        error_tok(vm, tok, "array designator index exceeds array bounds");

    if (equal(tok, "...")) {
        *end = const_expr(vm, &tok, tok->next);
        if (*end >= ty->array_len)
            error_tok(vm, tok, "array designator index exceeds array bounds");
        if (*end < *begin)
            error_tok(vm, tok, "array designator range [%d, %d] is empty", *begin, *end);
    } else {
        *end = *begin;
    }

    *rest = skip(vm, tok, "]");
}

// struct-designator = "." ident
static Member *struct_designator(JCC *vm, Token **rest, Token *tok, Type *ty) {
    Token *start = tok;
    tok = skip(vm, tok, ".");
    if (tok->kind != TK_IDENT)
        error_tok(vm, tok, "expected a field designator");

    for (Member *mem = ty->members; mem; mem = mem->next) {
        // Anonymous struct member
        if (mem->ty->kind == TY_STRUCT && !mem->name) {
            if (get_struct_member(mem->ty, tok)) {
                *rest = start;
                return mem;
            }
            continue;
        }

        // Regular struct member
        if (mem->name->len == tok->len && !strncmp(mem->name->loc, tok->loc, tok->len)) {
            *rest = tok->next;
            return mem;
        }
    }

    error_tok(vm, tok, "struct has no such member");
    return NULL;
}

// designation = ("[" const-expr "]" | "." ident)* "="? initializer
static void designation(JCC *vm, Token **rest, Token *tok, Initializer *init) {
    if (equal(tok, "[")) {
        if (init->ty->kind != TY_ARRAY)
            error_tok(vm, tok, "array index in non-array initializer");

        int begin, end;
        array_designator(vm, &tok, tok, init->ty, &begin, &end);

        Token *tok2;
        for (int i = begin; i <= end; i++)
            designation(vm, &tok2, tok, init->children[i]);
        array_initializer2(vm, rest, tok2, init, begin + 1);
        return;
    }

    if (equal(tok, ".") && init->ty->kind == TY_STRUCT) {
        Member *mem = struct_designator(vm, &tok, tok, init->ty);
        designation(vm, &tok, tok, init->children[mem->idx]);
        init->expr = NULL;
        
        // Only continue with struct_initializer2 if we're not immediately followed
        // by another designator (which might re-designate the same nested struct)
        // This allows {.tl.x = 10, .tl.y = 20} to work correctly
        if (!equal(tok, ",") || !equal(tok->next, ".")) {
            struct_initializer2(vm, rest, tok, init, mem->next);
        } else {
            *rest = tok;
        }
        return;
    }

    if (equal(tok, ".") && init->ty->kind == TY_UNION) {
        Member *mem = struct_designator(vm, &tok, tok, init->ty);
        init->mem = mem;
        designation(vm, rest, tok, init->children[mem->idx]);
        return;
    }

    if (equal(tok, "."))
        error_tok(vm, tok, "field name not in struct or union initializer");

    if (equal(tok, "="))
        tok = tok->next;
    initializer2(vm, rest, tok, init);
}

// An array length can be omitted if an array has an initializer
// (e.g. `int x[] = {1,2,3}`). If it's omitted, count the number
// of initializer elements.
static int count_array_init_elements(JCC *vm, Token *tok, Type *ty) {
    bool first = true;
    Initializer *dummy = new_initializer(ty->base, true);

    int i = 0, max = 0;

    while (!consume_end(&tok, tok)) {
        if (!first)
            tok = skip(vm, tok, ",");
        first = false;

        if (equal(tok, "[")) {
            i = const_expr(vm, &tok, tok->next);
            if (equal(tok, "..."))
                i = const_expr(vm, &tok, tok->next);
            tok = skip(vm, tok, "]");
            designation(vm, &tok, tok, dummy);
        } else {
            initializer2(vm, &tok, tok, dummy);
        }

        i++;
        max = MAX(max, i);
    }
    return max;
}

// array-initializer1 = "{" initializer ("," initializer)* ","? "}"
static void array_initializer1(JCC *vm, Token **rest, Token *tok, Initializer *init) {
    tok = skip(vm, tok, "{");

    if (init->is_flexible) {
        int len = count_array_init_elements(vm, tok, init->ty);
        *init = *new_initializer(array_of(init->ty->base, len), false);
    }

    bool first = true;

    if (init->is_flexible) {
        int len = count_array_init_elements(vm, tok, init->ty);
        *init = *new_initializer(array_of(init->ty->base, len), false);
    }

    for (int i = 0; !consume_end(rest, tok); i++) {
        if (!first)
            tok = skip(vm, tok, ",");
        first = false;

        if (equal(tok, "[")) {
            int begin, end;
            array_designator(vm, &tok, tok, init->ty, &begin, &end);

            Token *tok2;
            for (int j = begin; j <= end; j++)
                designation(vm, &tok2, tok, init->children[j]);
            tok = tok2;
            i = end;
            continue;
        }

        if (i < init->ty->array_len)
            initializer2(vm, &tok, tok, init->children[i]);
        else
            tok = skip_excess_element(vm, tok);
    }
}

// array-initializer2 = initializer ("," initializer)*
static void array_initializer2(JCC *vm, Token **rest, Token *tok, Initializer *init, int i) {
    if (init->is_flexible) {
        int len = count_array_init_elements(vm, tok, init->ty);
        *init = *new_initializer(array_of(init->ty->base, len), false);
    }

    for (; i < init->ty->array_len && !is_end(tok); i++) {
        Token *start = tok;
        if (i > 0)
            tok = skip(vm, tok, ",");

        if (equal(tok, "[") || equal(tok, ".")) {
            *rest = start;
            return;
        }

        initializer2(vm, &tok, tok, init->children[i]);
    }
    *rest = tok;
}

// struct-initializer1 = "{" initializer ("," initializer)* ","? "}"
static void struct_initializer1(JCC *vm, Token **rest, Token *tok, Initializer *init) {
    tok = skip(vm, tok, "{");

    Member *mem = init->ty->members;
    bool first = true;

    while (!consume_end(rest, tok)) {
        if (!first)
            tok = skip(vm, tok, ",");
        first = false;

        if (equal(tok, ".")) {
            mem = struct_designator(vm, &tok, tok, init->ty);
            designation(vm, &tok, tok, init->children[mem->idx]);
            mem = mem->next;
            continue;
        }

        if (mem) {
            initializer2(vm, &tok, tok, init->children[mem->idx]);
            mem = mem->next;
        } else {
            tok = skip_excess_element(vm, tok);
        }
    }
}

// struct-initializer2 = initializer ("," initializer)*
static void struct_initializer2(JCC *vm, Token **rest, Token *tok, Initializer *init, Member *mem) {
    bool first = true;

    for (; mem && !is_end(tok); mem = mem->next) {
        Token *start = tok;

        if (!first)
            tok = skip(vm, tok, ",");
        first = false;

        if (equal(tok, "[") || equal(tok, ".")) {
            *rest = start;
            return;
        }

        initializer2(vm, &tok, tok, init->children[mem->idx]);
    }
    *rest = tok;
}

static void union_initializer(JCC *vm, Token **rest, Token *tok, Initializer *init) {
    // Unlike structs, union initializers take only one initializer,
    // and that initializes the first union member by default.
    // You can initialize other member using a designated initializer.
    if (equal(tok, "{") && equal(tok->next, ".")) {
        Member *mem = struct_designator(vm, &tok, tok->next, init->ty);
        init->mem = mem;
        designation(vm, &tok, tok, init->children[mem->idx]);
        *rest = skip(vm, tok, "}");
        return;
    }

    init->mem = init->ty->members;

    if (equal(tok, "{")) {
        initializer2(vm, &tok, tok->next, init->children[0]);
        consume(vm, &tok, tok, ",");
        *rest = skip(vm, tok, "}");
    } else {
        initializer2(vm, rest, tok, init->children[0]);
    }
}

// initializer = string-initializer | array-initializer
//             | struct-initializer | union-initializer
//             | assign
static void initializer2(JCC *vm, Token **rest, Token *tok, Initializer *init) {
    if (init->ty->kind == TY_ARRAY && tok->kind == TK_STR) {
        string_initializer(rest, tok, init);
        return;
    }

    if (init->ty->kind == TY_ARRAY) {
        if (equal(tok, "{"))
            array_initializer1(vm, rest, tok, init);
        else
            array_initializer2(vm, rest, tok, init, 0);
        return;
    }

    if (init->ty->kind == TY_STRUCT) {
        if (equal(tok, "{")) {
            struct_initializer1(vm, rest, tok, init);
            return;
        }

        // A struct can be initialized with another struct. E.g.
        // `struct T x = y;` where y is a variable of type `struct T`.
        // Handle that case first.
        Node *expr = assign(vm, rest, tok);
        add_type(vm, expr);
        if (expr->ty->kind == TY_STRUCT) {
            init->expr = expr;
            return;
        }

        struct_initializer2(vm, rest, tok, init, init->ty->members);
        return;
    }

    if (init->ty->kind == TY_UNION) {
        if (equal(tok, "{")) {
            union_initializer(vm, rest, tok, init);
            return;
        }

        // A union can be initialized with another union. E.g.
        // `union T x = y;` where y is a variable or expression of type `union T`.
        // Handle that case first.
        Node *expr = assign(vm, rest, tok);
        add_type(vm, expr);
        if (expr->ty->kind == TY_UNION) {
            init->expr = expr;
            return;
        }

        // Otherwise, initialize the first member
        union_initializer(vm, rest, tok, init);
        return;
    }

    if (equal(tok, "{")) {
        // An initializer for a scalar variable can be surrounded by
        // braces. E.g. `int x = {3};`. Handle that case.
        initializer2(vm, &tok, tok->next, init);
        *rest = skip(vm, tok, "}");
        return;
    }

    init->expr = assign(vm, rest, tok);
}

static Type *copy_struct_type(Type *ty) {
    ty = copy_type(ty);

    Member head = {};
    Member *cur = &head;
    for (Member *mem = ty->members; mem; mem = mem->next) {
        Member *m = calloc(1, sizeof(Member));
        *m = *mem;
        cur = cur->next = m;
    }

    ty->members = head.next;
    return ty;
}

static Initializer *initializer(JCC *vm, Token **rest, Token *tok, Type *ty, Type **new_ty) {
    Initializer *init = new_initializer(ty, true);
    initializer2(vm, rest, tok, init);

    if ((ty->kind == TY_STRUCT || ty->kind == TY_UNION) && ty->is_flexible) {
        ty = copy_struct_type(ty);

        Member *mem = ty->members;
        while (mem->next)
            mem = mem->next;
        mem->ty = init->children[mem->idx]->ty;
        ty->size += mem->ty->size;

        *new_ty = ty;
        return init;
    }

    *new_ty = init->ty;
    return init;
}

static Node *init_desg_expr(JCC *vm, InitDesg *desg, Token *tok) {
    if (desg->var)
        return new_var_node(desg->var, tok);

    if (desg->member) {
        Node *node = new_unary(ND_MEMBER, init_desg_expr(vm, desg->next, tok), tok);
        node->member = desg->member;
        return node;
    }

    Node *lhs = init_desg_expr(vm, desg->next, tok);
    Node *rhs = new_num(desg->idx, tok);
    return new_unary(ND_DEREF, new_add(vm, lhs, rhs, tok), tok);
}

static Node *create_lvar_init(JCC *vm, Initializer *init, Type *ty, InitDesg *desg, Token *tok) {
    if (ty->kind == TY_ARRAY) {
        Node *node = new_node(ND_NULL_EXPR, tok);
        for (int i = 0; i < ty->array_len; i++) {
            InitDesg desg2 = {desg, i};
            Node *rhs = create_lvar_init(vm, init->children[i], ty->base, &desg2, tok);
            node = new_binary(ND_COMMA, node, rhs, tok);
        }
        return node;
    }

    if (ty->kind == TY_STRUCT && !init->expr) {
        Node *node = new_node(ND_NULL_EXPR, tok);

        for (Member *mem = ty->members; mem; mem = mem->next) {
            InitDesg desg2 = {desg, 0, mem};
            Node *rhs = create_lvar_init(vm, init->children[mem->idx], mem->ty, &desg2, tok);
            node = new_binary(ND_COMMA, node, rhs, tok);
        }
        return node;
    }

    if (ty->kind == TY_UNION && !init->expr) {
        Member *mem = init->mem ? init->mem : ty->members;
        InitDesg desg2 = {desg, 0, mem};
        return create_lvar_init(vm, init->children[mem->idx], mem->ty, &desg2, tok);
    }

    if (!init->expr)
        return new_node(ND_NULL_EXPR, tok);

    Node *lhs = init_desg_expr(vm, desg, tok);
    return new_binary(ND_ASSIGN, lhs, init->expr, tok);
}

// A variable definition with an initializer is a shorthand notation
// for a variable definition followed by assignments. This function
// generates assignment expressions for an initializer. For example,
// `int x[2][2] = {{6, 7}, {8, 9}}` is converted to the following
// expressions:
//
//   x[0][0] = 6;
//   x[0][1] = 7;
//   x[1][0] = 8;
//   x[1][1] = 9;
static Node *lvar_initializer(JCC *vm, Token **rest, Token *tok, Obj *var) {
    Initializer *init = initializer(vm, rest, tok, var->ty, &var->ty);
    InitDesg desg = {NULL, 0, NULL, var};

    // If a partial initializer list is given, the standard requires
    // that unspecified elements are set to 0. Here, we simply
    // zero-initialize the entire memory region of a variable before
    // initializing it with user-supplied values.
    Node *lhs = new_node(ND_MEMZERO, tok);
    lhs->var = var;

    Node *rhs = create_lvar_init(vm, init, var->ty, &desg, tok);
    return new_binary(ND_COMMA, lhs, rhs, tok);
}

static uint64_t read_buf(char *buf, int sz) {
    if (sz == 1)
        return *buf;
    if (sz == 2)
        return *(uint16_t *)buf;
    if (sz == 4)
        return *(uint32_t *)buf;
    if (sz == 8)
        return *(uint64_t *)buf;
    unreachable();
    return 0;
}

static void write_buf(char *buf, uint64_t val, int sz) {
    if (sz == 1)
        *buf = val;
    else if (sz == 2)
        *(uint16_t *)buf = val;
    else if (sz == 4)
        *(uint32_t *)buf = val;
    else if (sz == 8)
        *(uint64_t *)buf = val;
    else
        unreachable();
}

static Relocation *
write_gvar_data(JCC *vm, Relocation *cur, Initializer *init, Type *ty, char *buf, int offset) {
    if (ty->kind == TY_ARRAY) {
        int sz = ty->base->size;
        for (int i = 0; i < ty->array_len; i++)
            cur = write_gvar_data(vm, cur, init->children[i], ty->base, buf, offset + sz * i);
        return cur;
    }

    if (ty->kind == TY_STRUCT) {
        for (Member *mem = ty->members; mem; mem = mem->next) {
            if (mem->is_bitfield) {
                Node *expr = init->children[mem->idx]->expr;
                if (!expr)
                    break;

                char *loc = buf + offset + mem->offset;
                uint64_t oldval = read_buf(loc, mem->ty->size);
                uint64_t newval = eval(vm, expr);
                uint64_t mask = (1L << mem->bit_width) - 1;
                uint64_t combined = oldval | ((newval & mask) << mem->bit_offset);
                write_buf(loc, combined, mem->ty->size);
            } else {
                cur = write_gvar_data(vm, cur, init->children[mem->idx], mem->ty, buf,
                                      offset + mem->offset);
            }
        }
        return cur;
    }

    if (ty->kind == TY_UNION) {
        if (!init->mem)
            return cur;
        return write_gvar_data(vm, cur, init->children[init->mem->idx],
                               init->mem->ty, buf, offset);
    }

    if (!init->expr)
        return cur;

    if (ty->kind == TY_FLOAT) {
        *(float *)(buf + offset) = eval_double(vm, init->expr);
        return cur;
    }

    if (ty->kind == TY_DOUBLE) {
        *(double *)(buf + offset) = eval_double(vm, init->expr);
        return cur;
    }

    char **label = NULL;
    uint64_t val = eval2(vm, init->expr, &label);

    if (!label) {
        write_buf(buf + offset, val, ty->size);
        return cur;
    }

    Relocation *rel = calloc(1, sizeof(Relocation));
    rel->offset = offset;
    rel->label = label;
    rel->addend = val;
    cur->next = rel;
    return cur->next;
}

// Initializers for global variables are evaluated at compile-time and
// embedded to .data section. This function serializes Initializer
// objects to a flat byte array. It is a compile error if an
// initializer list contains a non-constant expression.
static void gvar_initializer(JCC *vm, Token **rest, Token *tok, Obj *var) {
    Initializer *init = initializer(vm, rest, tok, var->ty, &var->ty);

    // For constexpr variables, save the initializer expression for compile-time evaluation
    if (var->is_constexpr && init && init->expr) {
        var->init_expr = init->expr;
    }

    Relocation head = {};
    char *buf = calloc(1, var->ty->size);
    write_gvar_data(vm, &head, init, var->ty, buf, 0);
    var->init_data = buf;
    var->rel = head.next;
}

// Returns true if a given token represents a type.
static bool is_typename(JCC *vm, Token *tok) {
    static HashMap map;

    if (map.capacity == 0) {
        static char *kw[] = {
            "void", "_Bool", "char", "short", "int", "long", "struct", "union",
            "typedef", "enum", "static", "extern", "_Alignas", "signed", "unsigned",
            "const", "volatile", "auto", "register", "restrict", "__restrict",
            "__restrict__", "_Noreturn", "float", "double", "typeof", "inline",
            "_Thread_local", "__thread", "_Atomic", "constexpr",
        };

        for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
            hashmap_put(&map, kw[i], (void *)1);
    }

    return hashmap_get2(&map, tok->loc, tok->len) || find_typedef(vm, tok);
}

// asm-stmt = "asm" ("volatile" | "inline")* "(" string-literal ")"
static Node *asm_stmt(JCC *vm, Token **rest, Token *tok) {
    Node *node = new_node(ND_ASM, tok);
    tok = tok->next;

    while (equal(tok, "volatile") || equal(tok, "inline"))
        tok = tok->next;

    tok = skip(vm, tok, "(");
    if (tok->kind != TK_STR || tok->ty->base->kind != TY_CHAR)
        error_tok(vm, tok, "expected string literal");
    node->asm_str = tok->str;
    *rest = skip(vm, tok->next, ")");
    return node;
}

// stmt = "return" expr? ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "switch" "(" expr ")" stmt
//      | "case" const-expr ("..." const-expr)? ":" stmt
//      | "default" ":" stmt
//      | "for" "(" expr-stmt expr? ";" expr? ")" stmt
//      | "while" "(" expr ")" stmt
//      | "do" stmt "while" "(" expr ")" ";"
//      | "asm" asm-stmt
//      | "goto" (ident | "*" expr) ";"
//      | "break" ";"
//      | "continue" ";"
//      | ident ":" stmt
//      | "{" compound-stmt
//      | expr-stmt
static Node *stmt(JCC *vm, Token **rest, Token *tok) {
    if (equal(tok, "_Static_assert")) {
        tok = skip(vm, tok->next, "(");
        long long val = const_expr(vm, &tok, tok);
        tok = skip(vm, tok, ",");
        if (tok->kind != TK_STR)
            error_tok(vm, tok, "expected string literal");
        if (!val)
            error_tok(vm, tok, "%s", tok->str);
        tok = skip(vm, tok->next, ")");
        *rest = skip(vm, tok, ";");
        return new_node(ND_BLOCK, tok);
    }

    if (equal(tok, "return")) {
        Node *node = new_node(ND_RETURN, tok);
        if (consume(vm, rest, tok->next, ";"))
            return node;

        Node *exp = expr(vm, &tok, tok->next);
        *rest = skip(vm, tok, ";");

        add_type(vm, exp);
        Type *ty = vm->current_fn->ty->return_ty;
        if (ty->kind != TY_STRUCT && ty->kind != TY_UNION)
            exp = new_cast(vm, exp, vm->current_fn->ty->return_ty);

        node->lhs = exp;
        return node;
    }

    if (equal(tok, "if")) {
        Node *node = new_node(ND_IF, tok);
        tok = skip(vm, tok->next, "(");
        node->cond = expr(vm, &tok, tok);
        tok = skip(vm, tok, ")");
        node->then = stmt(vm, &tok, tok);
        if (equal(tok, "else"))
            node->els = stmt(vm, &tok, tok->next);
        *rest = tok;
        return node;
    }

    if (equal(tok, "switch")) {
        Node *node = new_node(ND_SWITCH, tok);
        tok = skip(vm, tok->next, "(");
        node->cond = expr(vm, &tok, tok);
        tok = skip(vm, tok, ")");

        Node *sw = vm->current_switch;
        vm->current_switch = node;

        char *brk = vm->brk_label;
        vm->brk_label = node->brk_label = new_unique_name();

        node->then = stmt(vm, rest, tok);

        vm->current_switch = sw;
        vm->brk_label = brk;
        return node;
    }

    if (equal(tok, "case")) {
        if (!vm->current_switch)
            error_tok(vm, tok, "stray case");

        Node *node = new_node(ND_CASE, tok);
        int begin = const_expr(vm, &tok, tok->next);
        int end;

        if (equal(tok, "...")) {
            // [GNU] Case ranges, e.g. "case 1 ... 5:"
            end = const_expr(vm, &tok, tok->next);
            if (end < begin)
                error_tok(vm, tok, "empty case range specified");
        } else {
            end = begin;
        }

        tok = skip(vm, tok, ":");
        node->label = new_unique_name();
        node->lhs = stmt(vm, rest, tok);
        node->begin = begin;
        node->end = end;
        node->case_next = vm->current_switch->case_next;
        vm->current_switch->case_next = node;
        return node;
    }

    if (equal(tok, "default")) {
        if (!vm->current_switch)
            error_tok(vm, tok, "stray default");

        Node *node = new_node(ND_CASE, tok);
        tok = skip(vm, tok->next, ":");
        node->label = new_unique_name();
        node->lhs = stmt(vm, rest, tok);
        vm->current_switch->default_case = node;
        return node;
    }

    if (equal(tok, "for")) {
        Node *node = new_node(ND_FOR, tok);
        tok = skip(vm, tok->next, "(");

        enter_scope(vm);

        char *brk = vm->brk_label;
        char *cont = vm->cont_label;
        vm->brk_label = node->brk_label = new_unique_name();
        vm->cont_label = node->cont_label = new_unique_name();

        if (is_typename(vm, tok)) {
            Type *basety = declspec(vm, &tok, tok, NULL);
            node->init = declaration(vm, &tok, tok, basety, NULL);
        } else {
            node->init = expr_stmt(vm, &tok, tok);
        }

        if (!equal(tok, ";"))
            node->cond = expr(vm, &tok, tok);
        tok = skip(vm, tok, ";");

        if (!equal(tok, ")"))
            node->inc = expr(vm, &tok, tok);
        tok = skip(vm, tok, ")");

        node->then = stmt(vm, rest, tok);

        leave_scope(vm);
        vm->brk_label = brk;
        vm->cont_label = cont;
        return node;
    }

    if (equal(tok, "while")) {
        Node *node = new_node(ND_FOR, tok);
        tok = skip(vm, tok->next, "(");
        node->cond = expr(vm, &tok, tok);
        tok = skip(vm, tok, ")");

        char *brk = vm->brk_label;
        char *cont = vm->cont_label;
        vm->brk_label = node->brk_label = new_unique_name();
        vm->cont_label = node->cont_label = new_unique_name();

        node->then = stmt(vm, rest, tok);

        vm->brk_label = brk;
        vm->cont_label = cont;
        return node;
    }

    if (equal(tok, "do")) {
        Node *node = new_node(ND_DO, tok);

        char *brk = vm->brk_label;
        char *cont = vm->cont_label;
        vm->brk_label = node->brk_label = new_unique_name();
        vm->cont_label = node->cont_label = new_unique_name();

        node->then = stmt(vm, &tok, tok->next);

        vm->brk_label = brk;
        vm->cont_label = cont;

        tok = skip(vm, tok, "while");
        tok = skip(vm, tok, "(");
        node->cond = expr(vm, &tok, tok);
        tok = skip(vm, tok, ")");
        *rest = skip(vm, tok, ";");
        return node;
    }

    if (equal(tok, "asm"))
        return asm_stmt(vm, rest, tok);

    if (equal(tok, "goto")) {
        if (equal(tok->next, "*")) {
            // [GNU] `goto *ptr` jumps to the address specified by `ptr`.
            Node *node = new_node(ND_GOTO_EXPR, tok);
            node->lhs = expr(vm, &tok, tok->next->next);
            *rest = skip(vm, tok, ";");
            return node;
        }

        Node *node = new_node(ND_GOTO, tok);
        node->label = get_ident(vm, tok->next);
        node->goto_next = vm->gotos;
        vm->gotos = node;
        *rest = skip(vm, tok->next->next, ";");
        return node;
    }

    if (equal(tok, "break")) {
        if (!vm->brk_label)
            error_tok(vm, tok, "stray break");
        Node *node = new_node(ND_GOTO, tok);
        node->unique_label = vm->brk_label;
        *rest = skip(vm, tok->next, ";");
        return node;
    }

    if (equal(tok, "continue")) {
        if (!vm->cont_label)
            error_tok(vm, tok, "stray continue");
        Node *node = new_node(ND_GOTO, tok);
        node->unique_label = vm->cont_label;
        *rest = skip(vm, tok->next, ";");
        return node;
    }

    if (tok->kind == TK_IDENT && equal(tok->next, ":")) {
        Node *node = new_node(ND_LABEL, tok);
        node->label = strndup(tok->loc, tok->len);
        node->unique_label = new_unique_name();
        node->lhs = stmt(vm, rest, tok->next->next);
        node->goto_next = vm->labels;
        vm->labels = node;
        return node;
    }

    if (equal(tok, "{"))
        return compound_stmt(vm, rest, tok->next);

    return expr_stmt(vm, rest, tok);
}

// compound-stmt = (typedef | declaration | stmt)* "}"
static Node *compound_stmt(JCC *vm, Token **rest, Token *tok) {
    Node *node = new_node(ND_BLOCK, tok);
    Node head = {};
    Node *cur = &head;

    enter_scope(vm);

    while (!equal(tok, "}")) {
        if (is_typename(vm, tok) && !equal(tok->next, ":")) {
            VarAttr attr = {};
            Type *basety = declspec(vm, &tok, tok, &attr);

            if (attr.is_typedef) {
                tok = parse_typedef(vm, tok, basety);
                continue;
            }

            if (is_function(vm, tok)) {
                tok = function(vm, tok, basety, &attr);
                continue;
            }

            if (attr.is_extern) {
                tok = global_variable(vm, tok, basety, &attr);
                continue;
            }

            cur = cur->next = declaration(vm, &tok, tok, basety, &attr);
        } else {
            // Clear initializing_var when we start parsing statements (non-declarations)
            // This ensures const variables can be initialized but not assigned later
            vm->initializing_var = NULL;
            cur = cur->next = stmt(vm, &tok, tok);
        }
        add_type(vm, cur);
    }

    // Also clear at end in case there are no statements after declarations
    vm->initializing_var = NULL;

    leave_scope(vm);

    node->body = head.next;
    *rest = tok->next;
    return node;
}

// expr-stmt = expr? ";"
static Node *expr_stmt(JCC *vm, Token **rest, Token *tok) {
    if (equal(tok, ";")) {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok);
    }

    Node *node = new_node(ND_EXPR_STMT, tok);
    node->lhs = expr(vm, &tok, tok);
    *rest = skip(vm, tok, ";");
    return node;
}

// expr = assign ("," expr)?
static Node *expr(JCC *vm, Token **rest, Token *tok) {
    Node *node = assign(vm, &tok, tok);

    if (equal(tok, ","))
        return new_binary(ND_COMMA, node, expr(vm, rest, tok->next), tok);

    *rest = tok;
    return node;
}

static int64_t eval(JCC *vm, Node *node) {
    return eval2(vm, node, NULL);
}

// Evaluate a given node as a constant expression.
//
// A constant expression is either just a number or ptr+n where ptr
// is a pointer to a global variable and n is a postiive/negative
// number. The latter form is accepted only as an initialization
// expression for a global variable.
static int64_t eval2(JCC *vm, Node *node, char ***label) {
    add_type(vm, node);

    if (is_flonum(node->ty))
        return eval_double(vm, node);

    switch (node->kind) {
        case ND_ADD:
            return eval2(vm, node->lhs, label) + eval(vm, node->rhs);
        case ND_SUB:
            return eval2(vm, node->lhs, label) - eval(vm, node->rhs);
        case ND_MUL:
            return eval(vm, node->lhs) * eval(vm, node->rhs);
        case ND_DIV:
            if (node->ty->is_unsigned)
                return (uint64_t)eval(vm, node->lhs) / eval(vm, node->rhs);
            return eval(vm, node->lhs) / eval(vm, node->rhs);
        case ND_NEG:
            return -eval(vm, node->lhs);
        case ND_MOD:
            if (node->ty->is_unsigned)
                return (uint64_t)eval(vm, node->lhs) % eval(vm, node->rhs);
            return eval(vm, node->lhs) % eval(vm, node->rhs);
        case ND_BITAND:
            return eval(vm, node->lhs) & eval(vm, node->rhs);
        case ND_BITOR:
            return eval(vm, node->lhs) | eval(vm, node->rhs);
        case ND_BITXOR:
            return eval(vm, node->lhs) ^ eval(vm, node->rhs);
        case ND_SHL:
            return eval(vm, node->lhs) << eval(vm, node->rhs);
        case ND_SHR:
            if (node->ty->is_unsigned && node->ty->size == 8)
                return (uint64_t)eval(vm, node->lhs) >> eval(vm, node->rhs);
            return eval(vm, node->lhs) >> eval(vm, node->rhs);
        case ND_EQ:
            return eval(vm, node->lhs) == eval(vm, node->rhs);
        case ND_NE:
            return eval(vm, node->lhs) != eval(vm, node->rhs);
        case ND_LT:
            if (node->lhs->ty->is_unsigned)
                return (uint64_t)eval(vm, node->lhs) < eval(vm, node->rhs);
            return eval(vm, node->lhs) < eval(vm, node->rhs);
        case ND_LE:
            if (node->lhs->ty->is_unsigned)
                return (uint64_t)eval(vm, node->lhs) <= eval(vm, node->rhs);
            return eval(vm, node->lhs) <= eval(vm, node->rhs);
        case ND_COND:
            return eval(vm, node->cond) ? eval2(vm, node->then, label) : eval2(vm, node->els, label);
        case ND_COMMA:
            return eval2(vm, node->rhs, label);
        case ND_NOT:
            return !eval(vm, node->lhs);
        case ND_BITNOT:
            return ~eval(vm, node->lhs);
        case ND_LOGAND:
            return eval(vm, node->lhs) && eval(vm, node->rhs);
        case ND_LOGOR:
            return eval(vm, node->lhs) || eval(vm, node->rhs);
        case ND_CAST: {
            int64_t val = eval2(vm, node->lhs, label);
            if (is_integer(node->ty)) {
                switch (node->ty->size) {
                    case 1: return node->ty->is_unsigned ? (uint8_t)val : (int8_t)val;
                    case 2: return node->ty->is_unsigned ? (uint16_t)val : (int16_t)val;
                    case 4: return node->ty->is_unsigned ? (uint32_t)val : (int32_t)val;
                }
            }
            return val;
        }
        case ND_ADDR:
            return eval_rval(vm, node->lhs, label);
        case ND_LABEL_VAL:
            *label = &node->unique_label;
            return 0;
        case ND_MEMBER:
            if (!label)
                error_tok(vm, node->tok, "not a compile-time constant");
            if (node->ty->kind != TY_ARRAY)
                error_tok(vm, node->tok, "invalid initializer");
            return eval_rval(vm, node->lhs, label) + node->member->offset;
        case ND_VAR:
            if (!label)
                error_tok(vm, node->tok, "not a compile-time constant");
            if (node->var->ty->kind != TY_ARRAY && node->var->ty->kind != TY_FUNC)
                error_tok(vm, node->tok, "invalid initializer");
            *label = &node->var->name;
            return 0;
        case ND_NUM:
            return node->val;
    }

    error_tok(vm, node->tok, "not a compile-time constant");
    return 0;
}

static int64_t eval_rval(JCC *vm, Node *node, char ***label) {
    switch (node->kind) {
        case ND_VAR:
            if (node->var->is_local)
                error_tok(vm, node->tok, "not a compile-time constant");
            *label = &node->var->name;
            return 0;
        case ND_DEREF:
            return eval2(vm, node->lhs, label);
        case ND_MEMBER:
            return eval_rval(vm, node->lhs, label) + node->member->offset;
    }

    error_tok(vm, node->tok, "invalid initializer");
    return 0;
}

static bool is_const_expr(JCC *vm, Node *node) {
    add_type(vm, node);

    switch (node->kind) {
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_BITAND:
        case ND_BITOR:
        case ND_BITXOR:
        case ND_SHL:
        case ND_SHR:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_LOGAND:
        case ND_LOGOR:
            return is_const_expr(vm, node->lhs) && is_const_expr(vm, node->rhs);
        case ND_COND:
            if (!is_const_expr(vm, node->cond))
                return false;
            return is_const_expr(vm, eval(vm, node->cond) ? node->then : node->els);
        case ND_COMMA:
            return is_const_expr(vm, node->rhs);
        case ND_NEG:
        case ND_NOT:
        case ND_BITNOT:
        case ND_CAST:
            return is_const_expr(vm, node->lhs);
        case ND_NUM:
            return true;
    }

    return false;
}

int64_t const_expr(JCC *vm, Token **rest, Token *tok) {
    Node *node = conditional(vm, rest, tok);
    return eval(vm, node);
}

static double eval_double(JCC *vm, Node *node) {
    add_type(vm, node);

    if (is_integer(node->ty)) {
        if (node->ty->is_unsigned)
            return (unsigned long)eval(vm, node);
        return eval(vm, node);
    }

    switch (node->kind) {
        case ND_ADD:
            return eval_double(vm, node->lhs) + eval_double(vm, node->rhs);
        case ND_SUB:
            return eval_double(vm, node->lhs) - eval_double(vm, node->rhs);
        case ND_MUL:
            return eval_double(vm, node->lhs) * eval_double(vm, node->rhs);
        case ND_DIV:
            return eval_double(vm, node->lhs) / eval_double(vm, node->rhs);
        case ND_NEG:
            return -eval_double(vm, node->lhs);
        case ND_COND:
            return eval_double(vm, node->cond) ? eval_double(vm, node->then) : eval_double(vm, node->els);
        case ND_COMMA:
            return eval_double(vm, node->rhs);
        case ND_CAST:
            if (is_flonum(node->lhs->ty))
                return eval_double(vm, node->lhs);
            return eval(vm, node->lhs);
        case ND_NUM:
            return node->fval;
    }

    error_tok(vm, node->tok, "not a compile-time constant");
    return 0;
}

// Convert op= operators to expressions containing an assignment.
//
// In general, `A op= C` is converted to ``tmp = &A, *tmp = *tmp op B`.
// However, if a given expression is of form `A.x op= C`, the input is
// converted to `tmp = &A, (*tmp).x = (*tmp).x op C` to handle assignments
// to bitfields.
static Node *to_assign(JCC *vm, Node *binary) {
    add_type(vm, binary->lhs);
    add_type(vm, binary->rhs);
    Token *tok = binary->tok;

    // Convert `A.x op= C` to `tmp = &A, (*tmp).x = (*tmp).x op C`.
    if (binary->lhs->kind == ND_MEMBER) {
        Obj *var = new_lvar(vm, "", pointer_to(binary->lhs->lhs->ty));

        Node *expr1 = new_binary(ND_ASSIGN, new_var_node(var, tok),
                                 new_unary(ND_ADDR, binary->lhs->lhs, tok), tok);

        Node *expr2 = new_unary(ND_MEMBER,
                                new_unary(ND_DEREF, new_var_node(var, tok), tok),
                                tok);
        expr2->member = binary->lhs->member;

        Node *expr3 = new_unary(ND_MEMBER,
                                new_unary(ND_DEREF, new_var_node(var, tok), tok),
                                tok);
        expr3->member = binary->lhs->member;

        Node *expr4 = new_binary(ND_ASSIGN, expr2,
                                 new_binary(binary->kind, expr3, binary->rhs, tok),
                                 tok);

        return new_binary(ND_COMMA, expr1, expr4, tok);
    }

    // If A is an atomic type, Convert `A op= B` to
    //
    // ({
    //   T1 *addr = &A; T2 val = (B); T1 old = *addr; T1 new;
    //   do {
    //    new = old op val;
    //   } while (!atomic_compare_exchange_strong(addr, &old, new));
    //   new;
    // })
    if (binary->lhs->ty->is_atomic) {
        Node head = {};
        Node *cur = &head;

        Obj *addr = new_lvar(vm, "", pointer_to(binary->lhs->ty));
        Obj *val = new_lvar(vm, "", binary->rhs->ty);
        Obj *old = new_lvar(vm, "", binary->lhs->ty);
        Obj *new = new_lvar(vm, "", binary->lhs->ty);

        cur = cur->next =
        new_unary(ND_EXPR_STMT,
                  new_binary(ND_ASSIGN, new_var_node(addr, tok),
                             new_unary(ND_ADDR, binary->lhs, tok), tok),
                  tok);

        cur = cur->next =
        new_unary(ND_EXPR_STMT,
                  new_binary(ND_ASSIGN, new_var_node(val, tok), binary->rhs, tok),
                  tok);

        cur = cur->next =
        new_unary(ND_EXPR_STMT,
                  new_binary(ND_ASSIGN, new_var_node(old, tok),
                             new_unary(ND_DEREF, new_var_node(addr, tok), tok), tok),
                  tok);

        Node *loop = new_node(ND_DO, tok);
        loop->brk_label = new_unique_name();
        loop->cont_label = new_unique_name();

        Node *body = new_binary(ND_ASSIGN,
                                new_var_node(new, tok),
                                new_binary(binary->kind, new_var_node(old, tok),
                                           new_var_node(val, tok), tok),
                                tok);

        loop->then = new_node(ND_BLOCK, tok);
        loop->then->body = new_unary(ND_EXPR_STMT, body, tok);

        Node *cas = new_node(ND_CAS, tok);
        cas->cas_addr = new_var_node(addr, tok);
        cas->cas_old = new_unary(ND_ADDR, new_var_node(old, tok), tok);
        cas->cas_new = new_var_node(new, tok);
        loop->cond = new_unary(ND_NOT, cas, tok);

        cur = cur->next = loop;
        cur = cur->next = new_unary(ND_EXPR_STMT, new_var_node(new, tok), tok);

        Node *node = new_node(ND_STMT_EXPR, tok);
        node->body = head.next;
        return node;
    }

    // Convert `A op= B` to ``tmp = &A, *tmp = *tmp op B`.
    Obj *var = new_lvar(vm, "", pointer_to(binary->lhs->ty));

    Node *expr1 = new_binary(ND_ASSIGN, new_var_node(var, tok),
                             new_unary(ND_ADDR, binary->lhs, tok), tok);

    Node *expr2 =
    new_binary(ND_ASSIGN,
               new_unary(ND_DEREF, new_var_node(var, tok), tok),
               new_binary(binary->kind,
                          new_unary(ND_DEREF, new_var_node(var, tok), tok),
                          binary->rhs,
                          tok),
               tok);

    return new_binary(ND_COMMA, expr1, expr2, tok);
}

// assign    = conditional (assign-op assign)?
// assign-op = "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^="
//           | "<<=" | ">>="
static Node *assign(JCC *vm, Token **rest, Token *tok) {
    Node *node = conditional(vm, &tok, tok);

    if (equal(tok, "="))
        return new_binary(ND_ASSIGN, node, assign(vm, rest, tok->next), tok);

    if (equal(tok, "+="))
        return to_assign(vm, new_add(vm, node, assign(vm, rest, tok->next), tok));

    if (equal(tok, "-="))
        return to_assign(vm, new_sub(vm, node, assign(vm, rest, tok->next), tok));

    if (equal(tok, "*="))
        return to_assign(vm, new_binary(ND_MUL, node, assign(vm, rest, tok->next), tok));

    if (equal(tok, "/="))
        return to_assign(vm, new_binary(ND_DIV, node, assign(vm, rest, tok->next), tok));

    if (equal(tok, "%="))
        return to_assign(vm, new_binary(ND_MOD, node, assign(vm, rest, tok->next), tok));

    if (equal(tok, "&="))
        return to_assign(vm, new_binary(ND_BITAND, node, assign(vm, rest, tok->next), tok));

    if (equal(tok, "|="))
        return to_assign(vm, new_binary(ND_BITOR, node, assign(vm, rest, tok->next), tok));

    if (equal(tok, "^="))
        return to_assign(vm, new_binary(ND_BITXOR, node, assign(vm, rest, tok->next), tok));

    if (equal(tok, "<<="))
        return to_assign(vm, new_binary(ND_SHL, node, assign(vm, rest, tok->next), tok));

    if (equal(tok, ">>="))
        return to_assign(vm, new_binary(ND_SHR, node, assign(vm, rest, tok->next), tok));

    *rest = tok;
    return node;
}

// conditional = logor ("?" expr? ":" conditional)?
static Node *conditional(JCC *vm, Token **rest, Token *tok) {
    Node *cond = logor(vm, &tok, tok);

    if (!equal(tok, "?")) {
        *rest = tok;
        return cond;
    }

    if (equal(tok->next, ":")) {
        // [GNU] Compile `a ?: b` as `tmp = a, tmp ? tmp : b`.
        add_type(vm, cond);
        Obj *var = new_lvar(vm, "", cond->ty);
        Node *lhs = new_binary(ND_ASSIGN, new_var_node(var, tok), cond, tok);
        Node *rhs = new_node(ND_COND, tok);
        rhs->cond = new_var_node(var, tok);
        rhs->then = new_var_node(var, tok);
        rhs->els = conditional(vm, rest, tok->next->next);
        return new_binary(ND_COMMA, lhs, rhs, tok);
    }

    Node *node = new_node(ND_COND, tok);
    node->cond = cond;
    node->then = expr(vm, &tok, tok->next);
    tok = skip(vm, tok, ":");
    node->els = conditional(vm, rest, tok);
    return node;
}

// logor = logand ("||" logand)*
static Node *logor(JCC *vm, Token **rest, Token *tok) {
    Node *node = logand(vm, &tok, tok);
    while (equal(tok, "||")) {
        Token *start = tok;
        node = new_binary(ND_LOGOR, node, logand(vm, &tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

// logand = bitor ("&&" bitor)*
static Node *logand(JCC *vm, Token **rest, Token *tok) {
    Node *node = bitor(vm, &tok, tok);
    while (equal(tok, "&&")) {
        Token *start = tok;
        node = new_binary(ND_LOGAND, node, bitor(vm, &tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

// bitor = bitxor ("|" bitxor)*
static Node *bitor(JCC *vm, Token **rest, Token *tok) {
    Node *node = bitxor(vm, &tok, tok);
    while (equal(tok, "|")) {
        Token *start = tok;
        node = new_binary(ND_BITOR, node, bitxor(vm, &tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

// bitxor = bitand ("^" bitand)*
static Node *bitxor(JCC *vm, Token **rest, Token *tok) {
    Node *node = bitand(vm, &tok, tok);
    while (equal(tok, "^")) {
        Token *start = tok;
        node = new_binary(ND_BITXOR, node, bitand(vm, &tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

// bitand = equality ("&" equality)*
static Node *bitand(JCC *vm, Token **rest, Token *tok) {
    Node *node = equality(vm, &tok, tok);
    while (equal(tok, "&")) {
        Token *start = tok;
        node = new_binary(ND_BITAND, node, equality(vm, &tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality(JCC *vm, Token **rest, Token *tok) {
    Node *node = relational(vm, &tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "==")) {
            node = new_binary(ND_EQ, node, relational(vm, &tok, tok->next), start);
            continue;
        }

        if (equal(tok, "!=")) {
            node = new_binary(ND_NE, node, relational(vm, &tok, tok->next), start);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
static Node *relational(JCC *vm, Token **rest, Token *tok) {
    Node *node = shift(vm, &tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "<")) {
            node = new_binary(ND_LT, node, shift(vm, &tok, tok->next), start);
            continue;
        }

        if (equal(tok, "<=")) {
            node = new_binary(ND_LE, node, shift(vm, &tok, tok->next), start);
            continue;
        }

        if (equal(tok, ">")) {
            node = new_binary(ND_LT, shift(vm, &tok, tok->next), node, start);
            continue;
        }

        if (equal(tok, ">=")) {
            node = new_binary(ND_LE, shift(vm, &tok, tok->next), node, start);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// shift = add ("<<" add | ">>" add)*
static Node *shift(JCC *vm, Token **rest, Token *tok) {
    Node *node = add(vm, &tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "<<")) {
            node = new_binary(ND_SHL, node, add(vm, &tok, tok->next), start);
            continue;
        }

        if (equal(tok, ">>")) {
            node = new_binary(ND_SHR, node, add(vm, &tok, tok->next), start);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// In C, `+` operator is overloaded to perform the pointer arithmetic.
// If p is a pointer, p+n adds not n but sizeof(*p)*n to the value of p,
// so that p+n points to the location n elements (not bytes) ahead of p.
// In other words, we need to scale an integer value before adding to a
// pointer value. This function takes care of the scaling.
static Node *new_add(JCC *vm, Node *lhs, Node *rhs, Token *tok) {
    add_type(vm, lhs);
    add_type(vm, rhs);

    // num + num
    if (is_numeric(lhs->ty) && is_numeric(rhs->ty))
        return new_binary(ND_ADD, lhs, rhs, tok);

    if (lhs->ty->base && rhs->ty->base)
        error_tok(vm, tok, "invalid operands");

    // Canonicalize `num + ptr` to `ptr + num`.
    if (!lhs->ty->base && rhs->ty->base) {
        Node *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    // VLA + num
    if (lhs->ty->base->kind == TY_VLA) {
        rhs = new_binary(ND_MUL, rhs, new_var_node(lhs->ty->base->vla_size, tok), tok);
        return new_binary(ND_ADD, lhs, rhs, tok);
    }

    // ptr + num
    rhs = new_binary(ND_MUL, rhs, new_long(get_vm_size(lhs->ty->base), tok), tok);
    return new_binary(ND_ADD, lhs, rhs, tok);
}

// Like `+`, `-` is overloaded for the pointer type.
static Node *new_sub(JCC *vm, Node *lhs, Node *rhs, Token *tok) {
    add_type(vm, lhs);
    add_type(vm, rhs);

    // num - num
    if (is_numeric(lhs->ty) && is_numeric(rhs->ty))
        return new_binary(ND_SUB, lhs, rhs, tok);

    // VLA + num
    if (lhs->ty->base->kind == TY_VLA) {
        rhs = new_binary(ND_MUL, rhs, new_var_node(lhs->ty->base->vla_size, tok), tok);
        add_type(vm, rhs);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = lhs->ty;
        return node;
    }

    // ptr - num
    if (lhs->ty->base && is_integer(rhs->ty)) {
        rhs = new_binary(ND_MUL, rhs, new_long(get_vm_size(lhs->ty->base), tok), tok);
        add_type(vm, rhs);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = lhs->ty;
        return node;
    }

    // ptr - ptr, which returns how many elements are between the two.
    if (lhs->ty->base && rhs->ty->base) {
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = ty_long;
        return new_binary(ND_DIV, node, new_num(lhs->ty->base->size, tok), tok);
    }

    error_tok(vm, tok, "invalid operands");
    return NULL;
}

// add = mul ("+" mul | "-" mul)*
static Node *add(JCC *vm, Token **rest, Token *tok) {
    Node *node = mul(vm, &tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "+")) {
            node = new_add(vm, node, mul(vm, &tok, tok->next), start);
            continue;
        }

        if (equal(tok, "-")) {
            node = new_sub(vm, node, mul(vm, &tok, tok->next), start);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// mul = cast ("*" cast | "/" cast | "%" cast)*
static Node *mul(JCC *vm, Token **rest, Token *tok) {
    Node *node = cast(vm, &tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "*")) {
            node = new_binary(ND_MUL, node, cast(vm, &tok, tok->next), start);
            continue;
        }

        if (equal(tok, "/")) {
            node = new_binary(ND_DIV, node, cast(vm, &tok, tok->next), start);
            continue;
        }

        if (equal(tok, "%")) {
            node = new_binary(ND_MOD, node, cast(vm, &tok, tok->next), start);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// cast = "(" type-name ")" cast | unary
static Node *cast(JCC *vm, Token **rest, Token *tok) {
    if (equal(tok, "(") && is_typename(vm, tok->next)) {
        Token *start = tok;
        Type *ty = typename(vm, &tok, tok->next);
        tok = skip(vm, tok, ")");

        // compound literal
        if (equal(tok, "{"))
            return unary(vm, rest, start);

        // type cast
        Node *node = new_cast(vm, cast(vm, &tok, tok), ty);
        node->tok = start;
        *rest = tok;
        return node;
    }

    return unary(vm, rest, tok);
}

// unary = ("+" | "-" | "*" | "&" | "!" | "~") cast
//       | ("++" | "--") unary
//       | "&&" ident
//       | postfix
static Node *unary(JCC *vm, Token **rest, Token *tok) {
    if (equal(tok, "+"))
        return cast(vm, rest, tok->next);

    if (equal(tok, "-"))
        return new_unary(ND_NEG, cast(vm, rest, tok->next), tok);

    if (equal(tok, "&")) {
        Node *lhs = cast(vm, rest, tok->next);
        add_type(vm, lhs);
        if (lhs->kind == ND_MEMBER && lhs->member->is_bitfield)
            error_tok(vm, tok, "cannot take address of bitfield");
        return new_unary(ND_ADDR, lhs, tok);
    }

    if (equal(tok, "*")) {
        // [https://www.sigbus.info/n1570#6.5.3.2p4] This is an oddity
        // in the C spec, but dereferencing a function shouldn't do
        // anything. If foo is a function, `*foo`, `**foo` or `*****foo`
        // are all equivalent to just `foo`.
        Node *node = cast(vm, rest, tok->next);
        add_type(vm, node);
        if (node->ty->kind == TY_FUNC)
            return node;
        return new_unary(ND_DEREF, node, tok);
    }

    if (equal(tok, "!"))
        return new_unary(ND_NOT, cast(vm, rest, tok->next), tok);

    if (equal(tok, "~"))
        return new_unary(ND_BITNOT, cast(vm, rest, tok->next), tok);

    // Read ++i as i+=1
    if (equal(tok, "++"))
        return to_assign(vm, new_add(vm, unary(vm, rest, tok->next), new_num(1, tok), tok));

    // Read --i as i-=1
    if (equal(tok, "--"))
        return to_assign(vm, new_sub(vm, unary(vm, rest, tok->next), new_num(1, tok), tok));

    // [GNU] labels-as-values
    if (equal(tok, "&&")) {
        Node *node = new_node(ND_LABEL_VAL, tok);
        node->label = get_ident(vm, tok->next);
        node->goto_next = vm->gotos;
        vm->gotos = node;
        *rest = tok->next->next;
        return node;
    }

    return postfix(vm, rest, tok);
}

// struct-members = (declspec declarator (","  declarator)* ";")*
static void struct_members(JCC *vm, Token **rest, Token *tok, Type *ty) {
    Member head = {};
    Member *cur = &head;
    int idx = 0;

    while (!equal(tok, "}")) {
        VarAttr attr = {};
        Type *basety = declspec(vm, &tok, tok, &attr);
        bool first = true;

        // Anonymous struct member
        if ((basety->kind == TY_STRUCT || basety->kind == TY_UNION) &&
            consume(vm, &tok, tok, ";")) {
            Member *mem = calloc(1, sizeof(Member));
            mem->ty = basety;
            mem->idx = idx++;
            mem->align = attr.align ? attr.align : mem->ty->align;
            cur = cur->next = mem;
            continue;
        }

        // Regular struct members
        while (!consume(vm, &tok, tok, ";")) {
            if (!first)
                tok = skip(vm, tok, ",");
            first = false;

            Member *mem = calloc(1, sizeof(Member));
            mem->ty = declarator(vm, &tok, tok, basety);
            mem->name = mem->ty->name;
            mem->idx = idx++;
            mem->align = attr.align ? attr.align : mem->ty->align;

            if (consume(vm, &tok, tok, ":")) {
                mem->is_bitfield = true;
                mem->bit_width = const_expr(vm, &tok, tok);
            }

            cur = cur->next = mem;
        }
    }

    // If the last element is an array of incomplete type, it's
    // called a "flexible array member". It should behave as if
    // if were a zero-sized array.
    if (cur != &head && cur->ty->kind == TY_ARRAY && cur->ty->array_len < 0) {
        cur->ty = array_of(cur->ty->base, 0);
        ty->is_flexible = true;
    }

    *rest = tok->next;
    ty->members = head.next;
}

// attribute = ("__attribute__" "(" "(" attribute-list ")" ")")*
// All attributes are accepted but most are ignored (only packed/aligned are used)
static Token *attribute_list(JCC *vm, Token *tok, Type *ty) {
    while (consume(vm, &tok, tok, "__attribute__")) {
        tok = skip(vm, tok, "(");
        tok = skip(vm, tok, "(");

        bool first = true;

        while (!consume(vm, &tok, tok, ")")) {
            if (!first)
                tok = skip(vm, tok, ",");
            first = false;

            // Handle packed attribute
            if (consume(vm, &tok, tok, "packed")) {
                if (ty)
                    ty->is_packed = true;
                continue;
            }

            // Handle aligned attribute
            if (consume(vm, &tok, tok, "aligned")) {
                if (equal(tok, "(")) {
                    tok = skip(vm, tok, "(");
                    int align = const_expr(vm, &tok, tok);
                    if (ty)
                        ty->align = align;
                    tok = skip(vm, tok, ")");
                }
                continue;
            }

            // Handle all other attributes - just consume and ignore them
            // This includes: unused, noreturn, const, pure, nonnull, warn_unused_result, etc.
            if (tok->kind == TK_IDENT) {
                tok = tok->next;
                
                // Handle attributes with parameters: attr(args...)
                if (equal(tok, "(")) {
                    int depth = 1;
                    tok = tok->next;
                    // Skip all tokens until matching closing paren
                    while (depth > 0) {
                        if (equal(tok, "("))
                            depth++;
                        else if (equal(tok, ")"))
                            depth--;
                        tok = tok->next;
                    }
                }
                continue;
            }

            // If we hit something unexpected, just skip it
            tok = tok->next;
        }

        tok = skip(vm, tok, ")");
    }

    return tok;
}

// struct-union-decl = attribute? ident? ("{" struct-members)?
static Type *struct_union_decl(JCC *vm, Token **rest, Token *tok) {
    Type *ty = struct_type();
    tok = attribute_list(vm, tok, ty);

    // Read a tag.
    Token *tag = NULL;
    if (tok->kind == TK_IDENT) {
        tag = tok;
        tok = tok->next;
    }

    if (tag && !equal(tok, "{")) {
        *rest = tok;

        Type *ty2 = find_tag(vm, tag);
        if (ty2)
            return ty2;

        ty->size = -1;
        push_tag_scope(vm, tag, ty);
        return ty;
    }

    tok = skip(vm, tok, "{");

    // Construct a struct object.
    struct_members(vm, &tok, tok, ty);
    *rest = attribute_list(vm, tok, ty);

    if (tag) {
        // If this is a redefinition, overwrite a previous type.
        // Otherwise, register the struct type.
        Type *ty2 = hashmap_get2(&vm->scope->tags, tag->loc, tag->len);
        if (ty2) {
            *ty2 = *ty;
            return ty2;
        }

        push_tag_scope(vm, tag, ty);
    }

    return ty;
}

// struct-decl = struct-union-decl
static Type *struct_decl(JCC *vm, Token **rest, Token *tok) {
    Type *ty = struct_union_decl(vm, rest, tok);
    ty->kind = TY_STRUCT;

    if (ty->size < 0)
        return ty;

    // Assign offsets within the struct to members.
    int bits = 0;

    for (Member *mem = ty->members; mem; mem = mem->next) {
        if (mem->is_bitfield && mem->bit_width == 0) {
            // Zero-width anonymous bitfield has a special meaning.
            // It affects only alignment.
            bits = align_to(bits, mem->ty->size * 8);
        } else if (mem->is_bitfield) {
            int sz = mem->ty->size;
            if (bits / (sz * 8) != (bits + mem->bit_width - 1) / (sz * 8))
                bits = align_to(bits, sz * 8);

            mem->offset = align_down(bits / 8, sz);
            mem->bit_offset = bits % (sz * 8);
            bits += mem->bit_width;
        } else {
            if (!ty->is_packed)
                bits = align_to(bits, mem->align * 8);
            mem->offset = bits / 8;
            bits += mem->ty->size * 8;
        }

        if (!ty->is_packed && ty->align < mem->align)
            ty->align = mem->align;
    }

    ty->size = align_to(bits, ty->align * 8) / 8;
    return ty;
}

// union-decl = struct-union-decl
static Type *union_decl(JCC *vm, Token **rest, Token *tok) {
    Type *ty = struct_union_decl(vm, rest, tok);
    ty->kind = TY_UNION;

    if (ty->size < 0)
        return ty;

    // If union, we don't have to assign offsets because they
    // are already initialized to zero. We need to compute the
    // alignment and the size though.
    for (Member *mem = ty->members; mem; mem = mem->next) {
        if (ty->align < mem->align)
            ty->align = mem->align;
        if (ty->size < mem->ty->size)
            ty->size = mem->ty->size;
    }
    ty->size = align_to(ty->size, ty->align);
    return ty;
}

// Find a struct member by name.
static Member *get_struct_member(Type *ty, Token *tok) {
    for (Member *mem = ty->members; mem; mem = mem->next) {
        // Anonymous struct member
        if ((mem->ty->kind == TY_STRUCT || mem->ty->kind == TY_UNION) &&
            !mem->name) {
            if (get_struct_member(mem->ty, tok))
                return mem;
            continue;
        }

        // Regular struct member
        if (mem->name->len == tok->len &&
            !strncmp(mem->name->loc, tok->loc, tok->len))
            return mem;
    }
    return NULL;
}

// Create a node representing a struct member access, such as foo.bar
// where foo is a struct and bar is a member name.
//
// C has a feature called "anonymous struct" which allows a struct to
// have another unnamed struct as a member like this:
//
//   struct { struct { int a; }; int b; } x;
//
// The members of an anonymous struct belong to the outer struct's
// member namespace. Therefore, in the above example, you can access
// member "a" of the anonymous struct as "x.a".
//
// This function takes care of anonymous structs.
static Node *struct_ref(JCC *vm, Node *node, Token *tok) {
    add_type(vm, node);
    if (node->ty->kind != TY_STRUCT && node->ty->kind != TY_UNION)
        error_tok(vm, node->tok, "not a struct nor a union");

    Type *ty = node->ty;

    for (;;) {
        Member *mem = get_struct_member(ty, tok);
        if (!mem)
            error_tok(vm, tok, "no such member");
        node = new_unary(ND_MEMBER, node, tok);
        node->member = mem;
        if (mem->name)
            break;
        ty = mem->ty;
    }
    return node;
}

// Convert A++ to `(typeof A)((A += 1) - 1)`
static Node *new_inc_dec(JCC *vm, Node *node, Token *tok, int addend) {
    add_type(vm, node);
    return new_cast(vm, new_add(vm, to_assign(vm, new_add(vm, node, new_num(addend, tok), tok)),
                                new_num(-addend, tok), tok),
                    node->ty);
}

// postfix = "(" type-name ")" "{" initializer-list "}"
//         = ident "(" func-args ")" postfix-tail*
//         | primary postfix-tail*
//
// postfix-tail = "[" expr "]"
//              | "(" func-args ")"
//              | "." ident
//              | "->" ident
//              | "++"
//              | "--"
static Node *postfix(JCC *vm, Token **rest, Token *tok) {
    if (equal(tok, "(") && is_typename(vm, tok->next)) {
        // Compound literal
        Token *start = tok;
        Type *ty = typename(vm, &tok, tok->next);
        tok = skip(vm, tok, ")");

        if (vm->scope->next == NULL) {
            Obj *var = new_anon_gvar(vm, ty);
            gvar_initializer(vm, rest, tok, var);
            return new_var_node(var, start);
        }

        Obj *var = new_lvar(vm, "", ty);
        Node *lhs = lvar_initializer(vm, rest, tok, var);
        Node *rhs = new_var_node(var, tok);
        return new_binary(ND_COMMA, lhs, rhs, start);
    }

    Node *node = primary(vm, &tok, tok);

    for (;;) {
        if (equal(tok, "(")) {
            node = funcall(vm, &tok, tok->next, node);
            continue;
        }

        if (equal(tok, "[")) {
            // x[y] is short for *(x+y)
            Token *start = tok;
            Node *idx = expr(vm, &tok, tok->next);
            tok = skip(vm, tok, "]");
            node = new_unary(ND_DEREF, new_add(vm, node, idx, start), start);
            continue;
        }

        if (equal(tok, ".")) {
            node = struct_ref(vm, node, tok->next);
            tok = tok->next->next;
            continue;
        }

        if (equal(tok, "->")) {
            // x->y is short for (*x).y
            node = new_unary(ND_DEREF, node, tok);
            node = struct_ref(vm, node, tok->next);
            tok = tok->next->next;
            continue;
        }

        if (equal(tok, "++")) {
            node = new_inc_dec(vm, node, tok, 1);
            tok = tok->next;
            continue;
        }

        if (equal(tok, "--")) {
            node = new_inc_dec(vm, node, tok, -1);
            tok = tok->next;
            continue;
        }

        *rest = tok;
        return node;
    }
}

// funcall = (assign ("," assign)*)? ")"
static Node *funcall(JCC *vm, Token **rest, Token *tok, Node *fn) {
    add_type(vm, fn);

    if (fn->ty->kind != TY_FUNC &&
        (fn->ty->kind != TY_PTR || fn->ty->base->kind != TY_FUNC))
        error_tok(vm, fn->tok, "not a function");

    Type *ty = (fn->ty->kind == TY_FUNC) ? fn->ty : fn->ty->base;
    Type *param_ty = ty->params;

    Node head = {};
    Node *cur = &head;

    while (!equal(tok, ")")) {
        if (cur != &head)
            tok = skip(vm, tok, ",");

        Node *arg = assign(vm, &tok, tok);
        add_type(vm, arg);

        if (!param_ty && !ty->is_variadic)
            error_tok(vm, tok, "too many arguments");

        if (param_ty) {
            if (param_ty->kind != TY_STRUCT && param_ty->kind != TY_UNION)
                arg = new_cast(vm, arg, param_ty);
            param_ty = param_ty->next;
        } else if (arg->ty->kind == TY_FLOAT) {
            // If parameter type is omitted (e.g. in "..."), float
            // arguments are promoted to double.
            arg = new_cast(vm, arg, ty_double);
        }

        cur = cur->next = arg;
    }

    if (param_ty)
        error_tok(vm, tok, "too few arguments");

    *rest = skip(vm, tok, ")");

    Node *node = new_unary(ND_FUNCALL, fn, tok);
    node->func_ty = ty;
    node->ty = ty->return_ty;
    node->args = head.next;

    // If a function returns a struct, it is caller's responsibility
    // to allocate a space for the return value.
    if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION)
        node->ret_buffer = new_lvar(vm, "", node->ty);
    return node;
}

// generic-selection = "(" assign "," generic-assoc ("," generic-assoc)* ")"
//
// generic-assoc = type-name ":" assign
//               | "default" ":" assign
static Node *generic_selection(JCC *vm, Token **rest, Token *tok) {
    Token *start = tok;
    tok = skip(vm, tok, "(");

    Node *ctrl = assign(vm, &tok, tok);
    add_type(vm, ctrl);

    Type *t1 = ctrl->ty;
    if (t1->kind == TY_FUNC)
        t1 = pointer_to(t1);
    else if (t1->kind == TY_ARRAY)
        t1 = pointer_to(t1->base);

    Node *ret = NULL;

    while (!consume(vm, rest, tok, ")")) {
        tok = skip(vm, tok, ",");

        if (equal(tok, "default")) {
            tok = skip(vm, tok->next, ":");
            Node *node = assign(vm, &tok, tok);
            if (!ret)
                ret = node;
            continue;
        }

        Type *t2 = typename(vm, &tok, tok);
        tok = skip(vm, tok, ":");
        Node *node = assign(vm, &tok, tok);
        if (is_compatible(t1, t2))
            ret = node;
    }

    if (!ret)
        error_tok(vm, start, "controlling expression type not compatible with"
                  " any generic association type");
    return ret;
}

// primary = "(" "{" stmt+ "}" ")"
//         | "(" expr ")"
//         | "sizeof" "(" type-name ")"
//         | "sizeof" unary
//         | "_Alignof" "(" type-name ")"
//         | "_Alignof" unary
//         | "_Generic" generic-selection
//         | "__jcc_types_compatible_p" "(" type-name, type-name, ")"
//         | "__jcc_reg_class" "(" type-name ")"
//         | ident
//         | str
//         | num
static Node *primary(JCC *vm, Token **rest, Token *tok) {
    Token *start = tok;

    if (equal(tok, "(") && equal(tok->next, "{")) {
        // This is a GNU statement expresssion.
        Node *node = new_node(ND_STMT_EXPR, tok);
        node->body = compound_stmt(vm, &tok, tok->next->next)->body;
        *rest = skip(vm, tok, ")");
        return node;
    }

    if (equal(tok, "(")) {
        Node *node = expr(vm, &tok, tok->next);
        *rest = skip(vm, tok, ")");
        return node;
    }

    if (equal(tok, "sizeof") && equal(tok->next, "(") && is_typename(vm, tok->next->next)) {
        Type *ty = typename(vm, &tok, tok->next->next);
        *rest = skip(vm, tok, ")");

        if (ty->kind == TY_VLA) {
            if (ty->vla_size)
                return new_var_node(ty->vla_size, tok);

            Node *lhs = compute_vla_size(vm, ty, tok);
            Node *rhs = new_var_node(ty->vla_size, tok);
            return new_binary(ND_COMMA, lhs, rhs, tok);
        }

        return new_ulong(ty->size, start);
    }

    if (equal(tok, "sizeof")) {
        Node *node = unary(vm, rest, tok->next);
        add_type(vm, node);
        if (node->ty->kind == TY_VLA)
            return new_var_node(node->ty->vla_size, tok);
        return new_ulong(node->ty->size, tok);
    }

    if (equal(tok, "_Alignof") && equal(tok->next, "(") && is_typename(vm, tok->next->next)) {
        Type *ty = typename(vm, &tok, tok->next->next);
        *rest = skip(vm, tok, ")");
        return new_ulong(ty->align, tok);
    }

    if (equal(tok, "_Alignof")) {
        Node *node = unary(vm, rest, tok->next);
        add_type(vm, node);
        return new_ulong(node->ty->align, tok);
    }

    if (equal(tok, "_Generic"))
        return generic_selection(vm, rest, tok->next);

    if (equal(tok, "__jcc_types_compatible_p")) {
        tok = skip(vm, tok->next, "(");
        Type *t1 = typename(vm, &tok, tok);
        tok = skip(vm, tok, ",");
        Type *t2 = typename(vm, &tok, tok);
        *rest = skip(vm, tok, ")");
        return new_num(is_compatible(t1, t2), start);
    }

    if (equal(tok, "__jcc_reg_class")) {
        tok = skip(vm, tok->next, "(");
        Type *ty = typename(vm, &tok, tok);
        *rest = skip(vm, tok, ")");

        if (is_integer(ty) || ty->kind == TY_PTR)
            return new_num(0, start);
        if (is_flonum(ty))
            return new_num(1, start);
        return new_num(2, start);
    }

    if (equal(tok, "__jcc_compare_and_swap")) {
        Node *node = new_node(ND_CAS, tok);
        tok = skip(vm, tok->next, "(");
        node->cas_addr = assign(vm, &tok, tok);
        tok = skip(vm, tok, ",");
        node->cas_old = assign(vm, &tok, tok);
        tok = skip(vm, tok, ",");
        node->cas_new = assign(vm, &tok, tok);
        *rest = skip(vm, tok, ")");
        return node;
    }

    if (equal(tok, "__jcc_atomic_exchange")) {
        Node *node = new_node(ND_EXCH, tok);
        tok = skip(vm, tok->next, "(");
        node->lhs = assign(vm, &tok, tok);
        tok = skip(vm, tok, ",");
        node->rhs = assign(vm, &tok, tok);
        *rest = skip(vm, tok, ")");
        return node;
    }

    if (tok->kind == TK_IDENT) {
        // Variable or enum constant
        VarScope *sc = find_var(vm, tok);
        *rest = tok->next;

        // For "static inline" function
        if (sc && sc->var && sc->var->is_function) {
            if (vm->current_fn)
                strarray_push(&vm->current_fn->refs, sc->var->name);
            else
                sc->var->is_root = true;
        }

        if (sc) {
            if (sc->var)
                return new_var_node(sc->var, tok);
            if (sc->enum_ty)
                return new_num(sc->enum_val, tok);
        }

        if (equal(tok->next, "("))
            error_tok(vm, tok, "implicit declaration of a function");
        error_tok(vm, tok, "undefined variable");
    }

    if (tok->kind == TK_STR) {
        Obj *var = new_string_literal(vm, tok->str, tok->ty);
        *rest = tok->next;
        return new_var_node(var, tok);
    }

    if (tok->kind == TK_NUM) {
        Node *node;
        if (is_flonum(tok->ty)) {
            node = new_node(ND_NUM, tok);
            node->fval = tok->fval;
        } else {
            node = new_num(tok->val, tok);
        }

        node->ty = tok->ty;
        *rest = tok->next;
        return node;
    }

    error_tok(vm, tok, "expected an expression");
    return NULL;
}

static Token *parse_typedef(JCC *vm, Token *tok, Type *basety) {
    bool first = true;

    while (!consume(vm, &tok, tok, ";")) {
        if (!first)
            tok = skip(vm, tok, ",");
        first = false;

        Type *ty = declarator(vm, &tok, tok, basety);
        if (!ty->name)
            error_tok(vm, ty->name_pos, "typedef name omitted");
        push_scope(vm, get_ident(vm, ty->name))->type_def = ty;
    }
    return tok;
}

static void create_param_lvars(JCC *vm, Type *param) {
    if (param) {
        create_param_lvars(vm, param->next);
        if (!param->name)
            error_tok(vm, param->name_pos, "parameter name omitted");
        new_lvar(vm, get_ident(vm, param->name), param);
    }
}

// This function matches gotos or labels-as-values with labels.
//
// We cannot resolve gotos as we parse a function because gotos
// can refer a label that appears later in the function.
// So, we need to do this after we parse the entire function.
static void resolve_goto_labels(JCC *vm) {
    for (Node *x = vm->gotos; x; x = x->goto_next) {
        for (Node *y = vm->labels; y; y = y->goto_next) {
            if (!strcmp(x->label, y->label)) {
                x->unique_label = y->unique_label;
                break;
            }
        }

        if (x->unique_label == NULL)
            error_tok(vm, x->tok->next, "use of undeclared label");
    }

    vm->gotos = vm->labels = NULL;
}

static Obj *find_func(JCC *vm, char *name) {
    Scope *sc = vm->scope;
    while (sc->next)
        sc = sc->next;

    VarScope *sc2 = hashmap_get(&sc->vars, name);
    if (sc2 && sc2->var && sc2->var->is_function)
        return sc2->var;
    return NULL;
}

static void mark_live(JCC *vm, Obj *var) {
    if (!var->is_function || var->is_live)
        return;
    var->is_live = true;

    for (int i = 0; i < var->refs.len; i++) {
        Obj *fn = find_func(vm, var->refs.data[i]);
        if (fn)
            mark_live(vm, fn);
    }
}

static Token *function(JCC *vm, Token *tok, Type *basety, VarAttr *attr) {
    Type *ty = declarator(vm, &tok, tok, basety);
    if (!ty->name)
        error_tok(vm, ty->name_pos, "function name omitted");
    char *name_str = get_ident(vm, ty->name);

    Obj *fn = find_func(vm, name_str);
    if (fn) {
        // Redeclaration
        if (!fn->is_function)
            error_tok(vm, tok, "redeclared as a different kind of symbol");
        if (fn->is_definition && equal(tok, "{"))
            error_tok(vm, tok, "redefinition of %s", name_str);
        if (!fn->is_static && attr->is_static)
            error_tok(vm, tok, "static declaration follows a non-static declaration");
        fn->is_definition = fn->is_definition || equal(tok, "{");
    } else {
        fn = new_gvar(vm, name_str, ty);
        fn->is_function = true;
        fn->is_definition = equal(tok, "{");
        fn->is_static = attr->is_static || (attr->is_inline && !attr->is_extern);
        fn->is_inline = attr->is_inline;
        fn->is_constexpr = attr->is_constexpr;
    }

    fn->is_root = !(fn->is_static && fn->is_inline);

    if (consume(vm, &tok, tok, ";"))
        return tok;

    vm->current_fn = fn;
    vm->locals = NULL;
    enter_scope(vm);
    create_param_lvars(vm, ty->params);

    // A buffer for a struct/union return value is passed
    // as the hidden first parameter.
    Type *rty = ty->return_ty;
    if ((rty->kind == TY_STRUCT || rty->kind == TY_UNION) && rty->size > 16)
        new_lvar(vm, "", pointer_to(rty));

    fn->params = vm->locals;

    if (ty->is_variadic)
        fn->va_area = new_lvar(vm, "__va_area__", array_of(ty_char, 136));
    fn->alloca_bottom = new_lvar(vm, "__alloca_size__", pointer_to(ty_char));

    tok = skip(vm, tok, "{");

    // [https://www.sigbus.info/n1570#6.4.2.2p1] "__func__" is
    // automatically defined as a local variable containing the
    // current function name.
    push_scope(vm, "__func__")->var =
    new_string_literal(vm, fn->name, array_of(ty_char, strlen(fn->name) + 1));

    // [GNU] __FUNCTION__ is yet another name of __func__.
    push_scope(vm, "__FUNCTION__")->var =
    new_string_literal(vm, fn->name, array_of(ty_char, strlen(fn->name) + 1));

    fn->body = compound_stmt(vm, &tok, tok);
    fn->locals = vm->locals;
    leave_scope(vm);
    resolve_goto_labels(vm);
    return tok;
}

static Token *global_variable(JCC *vm, Token *tok, Type *basety, VarAttr *attr) {
    bool first = true;

    while (!consume(vm, &tok, tok, ";")) {
        if (!first)
            tok = skip(vm, tok, ",");
        first = false;

        Type *ty = declarator(vm, &tok, tok, basety);
        if (!ty->name)
            error_tok(vm, ty->name_pos, "variable name omitted");

        Obj *var = new_gvar(vm, get_ident(vm, ty->name), ty);
        var->is_definition = !attr->is_extern;
        var->is_static = attr->is_static;
        var->is_tls = attr->is_tls;
        var->is_constexpr = attr->is_constexpr;
        if (attr->align)
            var->align = attr->align;

        if (equal(tok, "="))
            gvar_initializer(vm, &tok, tok->next, var);
        else if (!attr->is_extern && !attr->is_tls)
            var->is_tentative = true;
    }
    return tok;
}

// Lookahead tokens and returns true if a given token is a start
// of a function definition or declaration.
static bool is_function(JCC *vm, Token *tok) {
    if (equal(tok, ";"))
        return false;

    Type dummy = {};
    Type *ty = declarator(vm, &tok, tok, &dummy);
    return ty->kind == TY_FUNC;
}

// Remove redundant tentative definitions.
static void scan_globals(JCC *vm) {
    Obj head;
    Obj *cur = &head;

    for (Obj *var = vm->globals; var; var = var->next) {
        if (!var->is_tentative) {
            cur = cur->next = var;
            continue;
        }

        // Find another definition of the same identifier.
        Obj *var2 = vm->globals;
        for (; var2; var2 = var2->next)
            if (var != var2 && var2->is_definition && !strcmp(var->name, var2->name))
                break;

        // If there's another definition, the tentative definition
        // is redundant
        if (!var2)
            cur = cur->next = var;
    }

    cur->next = NULL;
    vm->globals = head.next;
}

static void declare_builtin_functions(JCC *vm) {
    Type *ty = func_type(pointer_to(ty_void));
    ty->params = copy_type(ty_int);
    vm->builtin_alloca = new_gvar(vm, "alloca", ty);
    vm->builtin_alloca->is_definition = false;
}

// program = (typedef | function-definition | global-variable)*
Obj *parse(JCC *vm, Token *tok) {
    // Initialize global scope
    enter_scope(vm);
    
    declare_builtin_functions(vm);
    vm->globals = NULL;

    while (tok->kind != TK_EOF) {
        VarAttr attr = {};
        Type *basety = declspec(vm, &tok, tok, &attr);

        // _Static_assert
        if (equal(tok, "_Static_assert")) {
            tok = skip(vm, tok->next, "(");
            long long val = const_expr(vm, &tok, tok);
            tok = skip(vm, tok, ",");
            if (tok->kind != TK_STR)
                error_tok(vm, tok, "expected string literal");
            if (!val)
                error_tok(vm, tok, "%s", tok->str);
            tok = skip(vm, tok->next, ")");
            tok = skip(vm, tok, ";");
            continue;
        }

        // Typedef
        if (attr.is_typedef) {
            tok = parse_typedef(vm, tok, basety);
            continue;
        }

        // Function
        if (is_function(vm, tok)) {
            tok = function(vm, tok, basety, &attr);
            continue;
        }

        // Global variable
        tok = global_variable(vm, tok, basety, &attr);
    }

    for (Obj *var = vm->globals; var; var = var->next)
        if (var->is_root)
            mark_live(vm, var);

    // Remove redundant tentative definitions.
    scan_globals(vm);
    return vm->globals;
}

// Exposed parsing functions for K's ast_parse API
Node *cc_parse_expr(JCC *vm, Token **rest, Token *tok) {
    return expr(vm, rest, tok);
}

Node *cc_parse_stmt(JCC *vm, Token **rest, Token *tok) {
    return stmt(vm, rest, tok);
}

Node *cc_parse_compound_stmt(JCC *vm, Token **rest, Token *tok) {
    return compound_stmt(vm, rest, tok);
}
