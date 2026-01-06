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

// AST to source code serialization
// Converts AST nodes back to C source text for -M pragma macro expansion output

#include "./internal.h"

// Operator precedence (higher = binds tighter)
static int get_precedence(NodeKind kind) {
    switch (kind) {
    case ND_COMMA:
        return 1;
    case ND_ASSIGN:
        return 2;
    case ND_COND:
        return 3;
    case ND_LOGOR:
        return 4;
    case ND_LOGAND:
        return 5;
    case ND_BITOR:
        return 6;
    case ND_BITXOR:
        return 7;
    case ND_BITAND:
        return 8;
    case ND_EQ:
    case ND_NE:
        return 9;
    case ND_LT:
    case ND_LE:
        return 10;
    case ND_SHL:
    case ND_SHR:
        return 11;
    case ND_ADD:
    case ND_SUB:
        return 12;
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
        return 13;
    case ND_NEG:
    case ND_NOT:
    case ND_BITNOT:
    case ND_ADDR:
    case ND_DEREF:
    case ND_CAST:
        return 14;
    case ND_FUNCALL:
    case ND_MEMBER:
        return 15;
    default:
        return 16;
    }
}

// Get operator string for binary operations
static const char *get_binary_op_str(NodeKind kind) {
    switch (kind) {
    case ND_ADD:
        return "+";
    case ND_SUB:
        return "-";
    case ND_MUL:
        return "*";
    case ND_DIV:
        return "/";
    case ND_MOD:
        return "%";
    case ND_BITAND:
        return "&";
    case ND_BITOR:
        return "|";
    case ND_BITXOR:
        return "^";
    case ND_SHL:
        return "<<";
    case ND_SHR:
        return ">>";
    case ND_EQ:
        return "==";
    case ND_NE:
        return "!=";
    case ND_LT:
        return "<";
    case ND_LE:
        return "<=";
    case ND_LOGAND:
        return "&&";
    case ND_LOGOR:
        return "||";
    case ND_ASSIGN:
        return "=";
    case ND_COMMA:
        return ",";
    default:
        return "?";
    }
}

// Get operator string for unary operations
static const char *get_unary_op_str(NodeKind kind) {
    switch (kind) {
    case ND_NEG:
        return "-";
    case ND_NOT:
        return "!";
    case ND_BITNOT:
        return "~";
    case ND_ADDR:
        return "&";
    case ND_DEREF:
        return "*";
    default:
        return "?";
    }
}

// Forward declaration
static void serialize_expr(FILE *f, JCC *vm, Node *node, int parent_prec);
static void serialize_stmt(FILE *f, JCC *vm, Node *node, int indent);

// Serialize type to string
static void serialize_type(FILE *f, Type *ty) {
    if (!ty) {
        fprintf(f, "void");
        return;
    }

    if (ty->is_const)
        fprintf(f, "const ");

    switch (ty->kind) {
    case TY_VOID:
        fprintf(f, "void");
        break;
    case TY_BOOL:
        fprintf(f, "_Bool");
        break;
    case TY_CHAR:
        fprintf(f, "%schar", ty->is_unsigned ? "unsigned " : "");
        break;
    case TY_SHORT:
        fprintf(f, "%sshort", ty->is_unsigned ? "unsigned " : "");
        break;
    case TY_INT:
        fprintf(f, "%sint", ty->is_unsigned ? "unsigned " : "");
        break;
    case TY_LONG:
        fprintf(f, "%slong", ty->is_unsigned ? "unsigned " : "");
        break;
    case TY_FLOAT:
        fprintf(f, "float");
        break;
    case TY_DOUBLE:
        fprintf(f, "double");
        break;
    case TY_LDOUBLE:
        fprintf(f, "long double");
        break;
    case TY_PTR:
        serialize_type(f, ty->base);
        fprintf(f, "*");
        break;
    case TY_ARRAY:
        serialize_type(f, ty->base);
        fprintf(f, "[%d]", ty->array_len);
        break;
    case TY_STRUCT:
        if (ty->name)
            fprintf(f, "struct %.*s", ty->name->len, ty->name->loc);
        else
            fprintf(f, "struct");
        break;
    case TY_UNION:
        if (ty->name)
            fprintf(f, "union %.*s", ty->name->len, ty->name->loc);
        else
            fprintf(f, "union");
        break;
    case TY_ENUM:
        if (ty->name)
            fprintf(f, "enum %.*s", ty->name->len, ty->name->loc);
        else
            fprintf(f, "enum");
        break;
    case TY_FUNC:
        serialize_type(f, ty->return_ty);
        fprintf(f, "(*)()");
        break;
    default:
        fprintf(f, "/* unknown type */");
        break;
    }
}

// Print escaped string literal
static void serialize_string(FILE *f, const char *str) {
    fprintf(f, "\"");
    for (const char *p = str; *p; p++) {
        switch (*p) {
        case '\n':
            fprintf(f, "\\n");
            break;
        case '\r':
            fprintf(f, "\\r");
            break;
        case '\t':
            fprintf(f, "\\t");
            break;
        case '\\':
            fprintf(f, "\\\\");
            break;
        case '"':
            fprintf(f, "\\\"");
            break;
        case '\0':
            fprintf(f, "\\0");
            break;
        default:
            if (*p >= 32 && *p < 127)
                fputc(*p, f);
            else
                fprintf(f, "\\x%02x", (unsigned char)*p);
            break;
        }
    }
    fprintf(f, "\"");
}

// Print indentation
static void print_indent_level(FILE *f, int indent) {
    for (int i = 0; i < indent; i++)
        fprintf(f, "    ");
}

// Serialize an expression
static void serialize_expr(FILE *f, JCC *vm, Node *node, int parent_prec) {
    (void)vm; // May be used later

    if (!node) {
        fprintf(f, "/* NULL */");
        return;
    }

    int node_prec = get_precedence(node->kind);
    bool need_parens = (node_prec < parent_prec);

    if (need_parens)
        fprintf(f, "(");

    switch (node->kind) {
    case ND_NUM:
        if (node->ty && is_flonum(node->ty))
            fprintf(f, "%Lg", node->fval);
        else
            fprintf(f, "%lld", (long long)node->val);
        break;

    case ND_VAR:
        if (node->var) {
            // Check if this is a string literal (anonymous global with
            // init_data)
            if (node->var->init_data && node->var->name[0] == '.') {
                serialize_string(f, node->var->init_data);
            } else {
                fprintf(f, "%s", node->var->name);
            }
        } else {
            fprintf(f, "/* unknown_var */");
        }
        break;

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
    case ND_COMMA:
        serialize_expr(f, vm, node->lhs, node_prec);
        fprintf(f, " %s ", get_binary_op_str(node->kind));
        serialize_expr(f, vm, node->rhs, node_prec + 1);
        break;

    case ND_NEG:
    case ND_NOT:
    case ND_BITNOT:
    case ND_ADDR:
    case ND_DEREF:
        fprintf(f, "%s", get_unary_op_str(node->kind));
        serialize_expr(f, vm, node->lhs, node_prec);
        break;

    case ND_CAST:
        fprintf(f, "(");
        serialize_type(f, node->ty);
        fprintf(f, ")");
        serialize_expr(f, vm, node->lhs, node_prec);
        break;

    case ND_COND:
        serialize_expr(f, vm, node->cond, 0);
        fprintf(f, " ? ");
        serialize_expr(f, vm, node->then, 0);
        fprintf(f, " : ");
        serialize_expr(f, vm, node->els, 0);
        break;

    case ND_FUNCALL:
        serialize_expr(f, vm, node->lhs, node_prec);
        fprintf(f, "(");
        for (Node *arg = node->args; arg; arg = arg->next) {
            serialize_expr(f, vm, arg, 0);
            if (arg->next)
                fprintf(f, ", ");
        }
        fprintf(f, ")");
        break;

    case ND_MEMBER:
        serialize_expr(f, vm, node->lhs, node_prec);
        if (node->member && node->member->name)
            fprintf(f, ".%.*s", node->member->name->len,
                    node->member->name->loc);
        else
            fprintf(f, "./* unknown */");
        break;

    case ND_STMT_EXPR:
        fprintf(f, "({\n");
        for (Node *s = node->body; s; s = s->next) {
            serialize_stmt(f, vm, s, 1);
        }
        fprintf(f, "})");
        break;

    case ND_NULL_EXPR:
        // Empty expression
        break;

    default:
        fprintf(f, "/* unsupported expr kind %d */", node->kind);
        break;
    }

    if (need_parens)
        fprintf(f, ")");
}

// Serialize a statement
static void serialize_stmt(FILE *f, JCC *vm, Node *node, int indent) {
    if (!node)
        return;

    switch (node->kind) {
    case ND_RETURN:
        print_indent_level(f, indent);
        fprintf(f, "return");
        if (node->lhs) {
            fprintf(f, " ");
            serialize_expr(f, vm, node->lhs, 0);
        }
        fprintf(f, ";\n");
        break;

    case ND_EXPR_STMT:
        print_indent_level(f, indent);
        serialize_expr(f, vm, node->lhs, 0);
        fprintf(f, ";\n");
        break;

    case ND_BLOCK:
        print_indent_level(f, indent);
        fprintf(f, "{\n");
        for (Node *s = node->body; s; s = s->next) {
            serialize_stmt(f, vm, s, indent + 1);
        }
        print_indent_level(f, indent);
        fprintf(f, "}\n");
        break;

    case ND_IF:
        print_indent_level(f, indent);
        fprintf(f, "if (");
        serialize_expr(f, vm, node->cond, 0);
        fprintf(f, ")\n");
        serialize_stmt(f, vm, node->then, indent + 1);
        if (node->els) {
            print_indent_level(f, indent);
            fprintf(f, "else\n");
            serialize_stmt(f, vm, node->els, indent + 1);
        }
        break;

    case ND_FOR:
        print_indent_level(f, indent);
        fprintf(f, "for (");
        if (node->init)
            serialize_expr(f, vm, node->init, 0);
        fprintf(f, "; ");
        if (node->cond)
            serialize_expr(f, vm, node->cond, 0);
        fprintf(f, "; ");
        if (node->inc)
            serialize_expr(f, vm, node->inc, 0);
        fprintf(f, ")\n");
        serialize_stmt(f, vm, node->then, indent + 1);
        break;

    case ND_DO:
        print_indent_level(f, indent);
        fprintf(f, "do\n");
        serialize_stmt(f, vm, node->then, indent + 1);
        print_indent_level(f, indent);
        fprintf(f, "while (");
        serialize_expr(f, vm, node->cond, 0);
        fprintf(f, ");\n");
        break;

    case ND_SWITCH:
        print_indent_level(f, indent);
        fprintf(f, "switch (");
        serialize_expr(f, vm, node->cond, 0);
        fprintf(f, ") {\n");
        for (Node *c = node->case_next; c; c = c->case_next) {
            print_indent_level(f, indent);
            fprintf(f, "case %ld:\n", c->begin);
            serialize_stmt(f, vm, c->body, indent + 1);
        }
        if (node->default_case) {
            print_indent_level(f, indent);
            fprintf(f, "default:\n");
            serialize_stmt(f, vm, node->default_case->body, indent + 1);
        }
        print_indent_level(f, indent);
        fprintf(f, "}\n");
        break;

    case ND_GOTO:
        print_indent_level(f, indent);
        fprintf(f, "goto %s;\n", node->label);
        break;

    case ND_LABEL:
        fprintf(f, "%s:\n", node->label);
        serialize_stmt(f, vm, node->lhs, indent);
        break;

    case ND_CASE:
        // Handled as part of switch
        break;

    default:
        // Treat as expression statement
        print_indent_level(f, indent);
        serialize_expr(f, vm, node, 0);
        fprintf(f, ";\n");
        break;
    }
}

// Serialize a function
static void serialize_function(FILE *f, JCC *vm, Obj *fn) {
    if (!fn->is_function)
        return;

    // Skip pragma macro functions (they were consumed)
    // Skip non-definitions
    if (!fn->is_definition && !fn->body)
        return;

    // Static keyword
    if (fn->is_static)
        fprintf(f, "static ");

    // Return type
    if (fn->ty && fn->ty->return_ty)
        serialize_type(f, fn->ty->return_ty);
    else
        fprintf(f, "int");

    fprintf(f, " %s(", fn->name);

    // Parameters
    bool first = true;
    for (Obj *param = fn->params; param; param = param->next) {
        if (!first)
            fprintf(f, ", ");
        first = false;
        serialize_type(f, param->ty);
        fprintf(f, " %s", param->name);
    }

    if (fn->ty && fn->ty->is_variadic) {
        if (!first)
            fprintf(f, ", ");
        fprintf(f, "...");
    }

    fprintf(f, ")");

    if (fn->body) {
        fprintf(f, " {\n");

        // Local variable declarations
        for (Obj *var = fn->locals; var; var = var->next) {
            if (var->is_param)
                continue;
            print_indent_level(f, 1);
            serialize_type(f, var->ty);
            fprintf(f, " %s;\n", var->name);
        }

        // Function body
        for (Node *s = fn->body; s; s = s->next) {
            serialize_stmt(f, vm, s, 1);
        }

        fprintf(f, "}\n\n");
    } else {
        fprintf(f, ";\n\n");
    }
}

// Serialize global variable
static void serialize_global_var(FILE *f, JCC *vm, Obj *var) {
    (void)vm;

    if (var->is_function)
        return;

    // Skip string literals (anonymous)
    if (var->name[0] == '.')
        return;

    if (var->is_static)
        fprintf(f, "static ");

    serialize_type(f, var->ty);
    fprintf(f, " %s", var->name);

    if (var->init_data) {
        fprintf(f, " = ");
        if (var->ty->kind == TY_ARRAY && var->ty->base->kind == TY_CHAR) {
            serialize_string(f, var->init_data);
        } else {
            fprintf(f, "/* init data */");
        }
    }

    fprintf(f, ";\n");
}

// Serialize struct/union type definition
static void serialize_struct_def(FILE *f, Type *ty) {
    if (!ty)
        return;

    if (ty->kind == TY_STRUCT)
        fprintf(f, "struct");
    else if (ty->kind == TY_UNION)
        fprintf(f, "union");
    else
        return;

    if (ty->name)
        fprintf(f, " %.*s", ty->name->len, ty->name->loc);

    fprintf(f, " {\n");
    for (Member *m = ty->members; m; m = m->next) {
        fprintf(f, "    ");
        serialize_type(f, m->ty);
        if (m->name)
            fprintf(f, " %.*s", m->name->len, m->name->loc);
        if (m->is_bitfield)
            fprintf(f, " : %d", m->bit_width);
        fprintf(f, ";\n");
    }
    fprintf(f, "};\n\n");
}

// Serialize enum type definition
static void serialize_enum_def(FILE *f, Type *ty) {
    if (!ty || ty->kind != TY_ENUM)
        return;

    fprintf(f, "enum");
    if (ty->name)
        fprintf(f, " %.*s", ty->name->len, ty->name->loc);

    fprintf(f, " {\n");
    for (EnumConstant *ec = ty->enum_constants; ec; ec = ec->next) {
        fprintf(f, "    %s = %d", ec->name, ec->value);
        if (ec->next)
            fprintf(f, ",");
        fprintf(f, "\n");
    }
    fprintf(f, "};\n\n");
}

// Public API: Serialize entire program to C source
void cc_serialize_program(FILE *f, JCC *vm, Obj *prog) {
    if (!f || !prog)
        return;

    // Header comment
    fprintf(f, "/* Generated by JCC pragma macro expansion */\n\n");

    // TODO: Collect and serialize type definitions
    // For now, we assume types are defined elsewhere or forward-declared

    // Serialize global variables
    for (Obj *obj = prog; obj; obj = obj->next) {
        if (!obj->is_function)
            serialize_global_var(f, vm, obj);
    }

    // Serialize functions
    for (Obj *obj = prog; obj; obj = obj->next) {
        if (obj->is_function)
            serialize_function(f, vm, obj);
    }
}

// Serialize a single node to a string (for debugging)
char *serialize_node_to_source(JCC *vm, Node *node) {
    if (!node)
        return strdup("");

    // Create a memory stream
    char *buffer = NULL;
    size_t size = 0;
    FILE *f = open_memstream(&buffer, &size);
    if (!f)
        return strdup("/* serialization error */");

    serialize_expr(f, vm, node, 0);
    fclose(f);

    return buffer;
}
