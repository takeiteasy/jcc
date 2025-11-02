/*
 JCC: JIT C Compiler - Reflection and AST Introspection API

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

/*!
 * @file reflection_api.h
 * @brief AST Reflection and Introspection API for JCC
 *
 * This header provides comprehensive APIs for:
 * - Type introspection and lookup
 * - Enum reflection
 * - Struct/union member introspection
 * - AST node construction (literals, expressions, statements)
 * - Control flow builders (switch, return, if, while, for)
 * - Function and struct construction
 *
 * All functions require an explicit JCC *vm parameter. For convenience
 * wrappers that hide the vm parameter in pragma macros, see pragma_api.h.
 */

#ifndef REFLECTION_H
#define REFLECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Type Lookup and Introspection
// ============================================================================

/*! Find a type by name. Returns NULL if not found. */
Type *ast_find_type(JCC *vm, const char *name);

/*! Check if a type exists by name. */
bool ast_type_exists(JCC *vm, const char *name);

/*! Lookup a type by name in the current scope (high-level wrapper). */
Type *ast_get_type(JCC *vm, const char *name);

/*! Get the TypeKind of a type. */
TypeKind ast_type_kind(Type *ty);

/*! Get sizeof() value in bytes. */
int ast_type_size(Type *ty);

/*! Get alignment in bytes. */
int ast_type_align(Type *ty);

/*! Check if type is unsigned. */
bool ast_type_is_unsigned(Type *ty);

/*! Check if type is const-qualified. */
bool ast_type_is_const(Type *ty);

/*! For pointer/array types: get the base type. Returns NULL if not applicable. */
Type *ast_type_base(Type *ty);

/*! For array types: get the array length. Returns -1 if not an array or is VLA. */
int ast_type_array_len(Type *ty);

/*! For function types: get return type. Returns NULL if not a function. */
Type *ast_type_return_type(Type *ty);

/*! For function types: get parameter count. Returns -1 if not a function. */
int ast_type_param_count(Type *ty);

/*! For function types: get parameter type at index. Returns NULL if out of bounds. */
Type *ast_type_param_at(Type *ty, int index);

/*! For function types: check if variadic. Returns false if not a function. */
bool ast_type_is_variadic(Type *ty);

/*! Get type name (for named types). Returns NULL if unnamed. */
const char *ast_type_name(Type *ty);

/*! Check if type is an integer type. */
bool ast_is_integer(Type *ty);

/*! Check if type is a floating-point type. */
bool ast_is_flonum(Type *ty);

/*! Check if type is a pointer type. */
bool ast_is_pointer(Type *ty);

/*! Check if type is an array type. */
bool ast_is_array(Type *ty);

/*! Check if type is a function type. */
bool ast_is_function(Type *ty);

/*! Check if type is a struct type. */
bool ast_is_struct(Type *ty);

/*! Check if type is a union type. */
bool ast_is_union(Type *ty);

/*! Check if type is an enum type. */
bool ast_is_enum(Type *ty);

/*! Create a pointer type to base. */
Type *ast_make_pointer(Type *base);

/*! Create an array type with specified length. */
Type *ast_make_array(Type *base, int length);

/*! Create a function type. */
Type *ast_make_function(Type *return_type, Type **param_types, int param_count, bool is_variadic);

// ============================================================================
// Enum Reflection
// ============================================================================

/*! Get the number of enum constants. Returns -1 if not an enum. */
int ast_enum_count(JCC *vm, Type *enum_type);

/*! Get enum constant at index (0-based). Returns NULL if out of bounds. */
EnumConstant *ast_enum_at(JCC *vm, Type *enum_type, int index);

/*! Find enum constant by name. Returns NULL if not found. */
EnumConstant *ast_enum_find(JCC *vm, Type *enum_type, const char *name);

/*! Get enum constant name. */
const char *ast_enum_constant_name(EnumConstant *ec);

/*! Get enum constant value. */
int ast_enum_constant_value(EnumConstant *ec);

/*! Get the name of an enum type. */
const char *ast_enum_name(Type *e);

/*! Get the number of values in an enum. */
size_t ast_enum_value_count(Type *e);

/*! Get the name of an enum value at index. */
const char *ast_enum_value_name(Type *e, size_t index);

/*! Get the integer value of an enum constant at index. */
int64_t ast_enum_value(Type *e, size_t index);

// ============================================================================
// Struct/Union Member Introspection
// ============================================================================

/*! Get the number of members. Returns -1 if not a struct/union. */
int ast_struct_member_count(JCC *vm, Type *struct_type);

/*! Get member at index (0-based). Returns NULL if out of bounds. */
Member *ast_struct_member_at(JCC *vm, Type *struct_type, int index);

/*! Find member by name. Returns NULL if not found. */
Member *ast_struct_member_find(JCC *vm, Type *struct_type, const char *name);

/*! Get member name. */
const char *ast_member_name(Member *m);

/*! Get member type. */
Type *ast_member_type(Member *m);

/*! Get member offset in bytes. */
int ast_member_offset(Member *m);

/*! Check if member is a bitfield. */
bool ast_member_is_bitfield(Member *m);

/*! Get bitfield width (returns 0 if not a bitfield). */
int ast_member_bitfield_width(Member *m);

// ============================================================================
// Global Symbol Introspection
// ============================================================================

/*! Find a global symbol by name. Returns NULL if not found. */
Obj *ast_find_global(JCC *vm, const char *name);

/*! Get the total number of global symbols. */
int ast_global_count(JCC *vm);

/*! Get global symbol at index (0-based). Returns NULL if out of bounds. */
Obj *ast_global_at(JCC *vm, int index);

/*! Get the name of an object (variable or function). */
const char *ast_obj_name(Obj *obj);

/*! Get the type of an object. */
Type *ast_obj_type(Obj *obj);

/*! Check if object is a function. */
bool ast_obj_is_function(Obj *obj);

/*! Check if object is a definition (not just a declaration). */
bool ast_obj_is_definition(Obj *obj);

/*! Check if object has static linkage. */
bool ast_obj_is_static(Obj *obj);

/*! For functions: get parameter count. Returns -1 if not a function. */
int ast_func_param_count(Obj *func);

/*! For functions: get parameter at index. Returns NULL if out of bounds. */
Obj *ast_func_param_at(Obj *func, int index);

/*! For functions: get function body AST. Returns NULL if no body. */
Node *ast_func_body(Obj *func);

// ============================================================================
// AST Node Construction - Literals
// ============================================================================

/*! Create a numeric literal node. */
Node *ast_node_num(JCC *vm, long long value);

/*! Create a floating-point literal node. */
Node *ast_node_float(JCC *vm, double value);

/*! Create a string literal node. */
Node *ast_node_string(JCC *vm, const char *str);

/*! Create an identifier (variable reference) node. */
Node *ast_node_ident(JCC *vm, const char *name);

/*! Create an integer literal node (high-level). */
Node *ast_int_literal(JCC *vm, int64_t value);

/*! Create a string literal node (high-level). */
Node *ast_string_literal(JCC *vm, const char *str);

/*! Create a variable reference node (high-level). */
Node *ast_var_ref(JCC *vm, const char *name);

// ============================================================================
// AST Node Construction - Expressions
// ============================================================================

/*! Create a binary operation node. */
Node *ast_node_binary(JCC *vm, NodeKind op, Node *left, Node *right);

/*! Create a unary operation node. */
Node *ast_node_unary(JCC *vm, NodeKind op, Node *operand);

/*! Create a function call node. */
Node *ast_node_call(JCC *vm, const char *func_name, Node **args, int arg_count);

/*! Create a member access node (struct.member or ptr->member). */
Node *ast_node_member(JCC *vm, Node *object, const char *member_name);

/*! Create a type cast node. */
Node *ast_node_cast(JCC *vm, Node *expr, Type *target_type);

// ============================================================================
// AST Node Construction - Statements
// ============================================================================

/*! Create a block (compound statement) node. */
Node *ast_node_block(JCC *vm, Node **stmts, int stmt_count);

/*! Create a return statement node. */
Node *ast_return(JCC *vm, Node *expr);

/*! Create a switch statement node. */
Node *ast_switch(JCC *vm, Node *condition);

/*! Add a case to a switch statement. */
void ast_switch_add_case(JCC *vm, Node *switch_node, Node *value, Node *body);

/*! Set the default case for a switch statement. */
void ast_switch_set_default(JCC *vm, Node *switch_node, Node *body);

// ============================================================================
// AST Node Construction - Declarations
// ============================================================================

/*! Create a function declaration/definition node. */
Node *ast_function(JCC *vm, const char *name, Type *return_type);

/*! Add a parameter to a function. */
void ast_function_add_param(JCC *vm, Node *func_node, const char *name, Type *type);

/*! Set the body of a function. */
void ast_function_set_body(JCC *vm, Node *func_node, Node *body);

/*! Create a struct type node. */
Node *ast_struct(JCC *vm, const char *name);

/*! Add a field to a struct. */
void ast_struct_add_field(JCC *vm, Node *struct_node, const char *name, Type *type);

// ============================================================================
// AST Node Introspection
// ============================================================================

/*! Get the NodeKind of a node. */
NodeKind ast_node_kind(Node *node);

/*! Get the type of a node (after type resolution). */
Type *ast_node_type(Node *node);

/*! For numeric literals: get integer value. */
long long ast_node_int_value(Node *node);

/*! For floating-point literals: get value. */
double ast_node_float_value(Node *node);

/*! For string literals: get string content. */
const char *ast_node_string_value(Node *node);

/*! Get left child node (for binary/unary operations). */
Node *ast_node_left(Node *node);

/*! Get right child node (for binary operations). */
Node *ast_node_right(Node *node);

/*! For function calls: get function name. */
const char *ast_node_func_name(Node *node);

/*! For function calls: get argument count. */
int ast_node_arg_count(Node *node);

/*! For function calls: get argument at index. */
Node *ast_node_arg_at(Node *node, int index);

/*! For blocks: get statement count. */
int ast_node_stmt_count(Node *node);

/*! For blocks: get statement at index. */
Node *ast_node_stmt_at(Node *node, int index);

/*! For variable references: get the Obj (variable/function object). */
Obj *ast_node_var(Node *node);

/*! Get the source token for a node (for error reporting). */
Token *ast_node_token(Node *node);

// ============================================================================
// Token Introspection
// ============================================================================

/*! Get the filename from a token. */
const char *ast_token_filename(Token *tok);

/*! Get the line number from a token. */
int ast_token_line(Token *tok);

/*! Get the column number from a token. */
int ast_token_column(Token *tok);

/*! Get the text content of a token. */
int ast_token_text(Token *tok, char *buffer, int bufsize);

#ifdef __cplusplus
}
#endif

#endif // REFLECTION_H
