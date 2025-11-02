// serialize.c - AST to source code serialization
// Converts AST nodes back to C source text for -E pragma macro expansion

#include "jcc.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
static char *serialize_node_impl(JCC *vm, Node *node, int parent_prec);
static char *strdup_printf(const char *fmt, ...);

// Operator precedence (higher = binds tighter)
// Based on C operator precedence table
static int get_precedence(NodeKind kind) {
    switch (kind) {
        // Lowest precedence
        case ND_COMMA:      return 1;
        case ND_ASSIGN:     return 2;
        case ND_COND:       return 3;   // ternary ?:
        case ND_LOGOR:      return 4;   // ||
        case ND_LOGAND:     return 5;   // &&
        case ND_BITOR:      return 6;   // |
        case ND_BITXOR:     return 7;   // ^
        case ND_BITAND:     return 8;   // &
        case ND_EQ:
        case ND_NE:         return 9;   // == !=
        case ND_LT:
        case ND_LE:         return 10;  // < <= > >=
        case ND_SHL:
        case ND_SHR:        return 11;  // << >>
        case ND_ADD:
        case ND_SUB:        return 12;  // + -
        case ND_MUL:
        case ND_DIV:
        case ND_MOD:        return 13;  // * / %
        // Unary operators
        case ND_NEG:
        case ND_NOT:
        case ND_BITNOT:
        case ND_ADDR:
        case ND_DEREF:      return 14;  // unary operators
        case ND_CAST:       return 14;  // type casts
        // Highest precedence
        case ND_FUNCALL:
        case ND_MEMBER:     return 15;  // () . ->
        default:            return 16;  // literals, identifiers
    }
}

// Get operator string for binary operations
static const char *get_binary_op_str(NodeKind kind) {
    switch (kind) {
        case ND_ADD:     return "+";
        case ND_SUB:     return "-";
        case ND_MUL:     return "*";
        case ND_DIV:     return "/";
        case ND_MOD:     return "%";
        case ND_BITAND:  return "&";
        case ND_BITOR:   return "|";
        case ND_BITXOR:  return "^";
        case ND_SHL:     return "<<";
        case ND_SHR:     return ">>";
        case ND_EQ:      return "==";
        case ND_NE:      return "!=";
        case ND_LT:      return "<";
        case ND_LE:      return "<=";
        case ND_LOGAND:  return "&&";
        case ND_LOGOR:   return "||";
        case ND_ASSIGN:  return "=";
        case ND_COMMA:   return ",";
        default:         return "?";
    }
}

// Get operator string for unary operations
static const char *get_unary_op_str(NodeKind kind) {
    switch (kind) {
        case ND_NEG:     return "-";
        case ND_NOT:     return "!";
        case ND_BITNOT:  return "~";
        case ND_ADDR:    return "&";
        case ND_DEREF:   return "*";
        default:         return "?";
    }
}

// Helper to create formatted string (like sprintf but allocates)
static char *strdup_printf(const char *fmt, ...) {
    va_list ap, ap_copy;
    va_start(ap, fmt);
    va_copy(ap_copy, ap);

    int size = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    char *result = malloc(size + 1);
    if (!result)
        return NULL;

    vsnprintf(result, size + 1, fmt, ap_copy);
    va_end(ap_copy);

    return result;
}

// Serialize type to string (for casts)
static char *serialize_type(Type *ty) {
    if (!ty)
        return strdup("void");

    switch (ty->kind) {
        case TY_VOID:   return strdup("void");
        case TY_BOOL:   return strdup("_Bool");
        case TY_CHAR:   return strdup("char");
        case TY_SHORT:  return strdup("short");
        case TY_INT:    return strdup("int");
        case TY_LONG:   return strdup("long");
        case TY_FLOAT:  return strdup("float");
        case TY_DOUBLE: return strdup("double");
        case TY_PTR: {
            char *base = serialize_type(ty->base);
            char *result = strdup_printf("%s*", base);
            free(base);
            return result;
        }
        default:
            return strdup("int");  // fallback
    }
}

// Main serialization function (with precedence handling)
static char *serialize_node_impl(JCC *vm, Node *node, int parent_prec) {
    if (!node)
        return strdup("");

    int node_prec = get_precedence(node->kind);
    bool need_parens = (node_prec < parent_prec);

    char *result = NULL;

    switch (node->kind) {
        case ND_NUM: {
            // Integer literal
            result = strdup_printf("%lld", node->val);
            break;
        }

        case ND_VAR: {
            // Variable/identifier
            if (node->var && node->var->name)
                result = strdup(node->var->name);
            else
                result = strdup("unknown_var");
            break;
        }

        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_MOD:
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
        case ND_ASSIGN:
        case ND_COMMA: {
            // Binary operators
            char *left = serialize_node_impl(vm, node->lhs, node_prec + 1);
            char *right = serialize_node_impl(vm, node->rhs, node_prec + 1);
            const char *op = get_binary_op_str(node->kind);

            if (need_parens)
                result = strdup_printf("(%s %s %s)", left, op, right);
            else
                result = strdup_printf("%s %s %s", left, op, right);

            free(left);
            free(right);
            break;
        }

        case ND_NEG:
        case ND_NOT:
        case ND_BITNOT:
        case ND_ADDR:
        case ND_DEREF: {
            // Unary operators
            const char *op = get_unary_op_str(node->kind);
            char *operand = serialize_node_impl(vm, node->lhs, node_prec);

            if (need_parens)
                result = strdup_printf("(%s%s)", op, operand);
            else
                result = strdup_printf("%s%s", op, operand);

            free(operand);
            break;
        }

        case ND_FUNCALL: {
            // Function call
            // Function name is in node->lhs (ND_VAR node)
            char *func_name = NULL;
            if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->var)
                func_name = strdup(node->lhs->var->name);
            else
                func_name = strdup("unknown_func");

            // Serialize arguments
            char *args_str = strdup("");
            for (Node *arg = node->args; arg; arg = arg->next) {
                char *arg_str = serialize_node_impl(vm, arg, 0);
                char *new_args = strdup_printf("%s%s%s", args_str,
                                               (args_str[0] ? ", " : ""), arg_str);
                free(args_str);
                free(arg_str);
                args_str = new_args;
            }

            result = strdup_printf("%s(%s)", func_name, args_str);
            free(func_name);
            free(args_str);
            break;
        }

        case ND_MEMBER: {
            // Member access: obj.member
            char *obj = serialize_node_impl(vm, node->lhs, node_prec);

            // Extract member name from token
            char *member_name = NULL;
            if (node->member && node->member->name) {
                // Copy the member name from the token
                int len = node->member->name->len;
                member_name = malloc(len + 1);
                strncpy(member_name, node->member->name->loc, len);
                member_name[len] = '\0';
            } else {
                member_name = strdup("unknown_member");
            }

            result = strdup_printf("%s.%s", obj, member_name);
            free(obj);
            free(member_name);
            break;
        }

        case ND_CAST: {
            // Type cast: (type)expr
            char *type_str = serialize_type(node->ty);
            char *expr = serialize_node_impl(vm, node->lhs, node_prec);
            result = strdup_printf("(%s)%s", type_str, expr);
            free(type_str);
            free(expr);
            break;
        }

        case ND_COND: {
            // Ternary operator: cond ? then : else
            char *cond = serialize_node_impl(vm, node->cond, 0);
            char *then_expr = serialize_node_impl(vm, node->then, 0);
            char *els_expr = serialize_node_impl(vm, node->els, 0);

            if (need_parens)
                result = strdup_printf("(%s ? %s : %s)", cond, then_expr, els_expr);
            else
                result = strdup_printf("%s ? %s : %s", cond, then_expr, els_expr);

            free(cond);
            free(then_expr);
            free(els_expr);
            break;
        }

        case ND_NULL_EXPR:
            result = strdup("");
            break;

        default:
            // Unsupported node type
            result = strdup_printf("/* unsupported node kind %d */", node->kind);
            break;
    }

    return result;
}

// Public API: Serialize an AST node to C source text
char *serialize_node_to_source(JCC *vm, Node *node) {
    return serialize_node_impl(vm, node, 0);
}
