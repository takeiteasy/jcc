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

#ifndef JCC_REFLECTION_H
#define JCC_REFLECTION_H

#include "jcc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! Find a type by name. Returns NULL if not found. */
Type *cc_find_type(JCC *vm, const char *name);

/*! Check if a type exists by name. */
bool cc_type_exists(JCC *vm, const char *name);


/*! Get the number of enum constants. Returns -1 if not an enum. */
int cc_enum_count(JCC *vm, Type *enum_type);

/*! Get enum constant at index (0-based). Returns NULL if out of bounds. */
EnumConstant *cc_enum_at(JCC *vm, Type *enum_type, int index);

/*! Find enum constant by name. Returns NULL if not found. */
EnumConstant *cc_enum_find(JCC *vm, Type *enum_type, const char *name);

/*! Get enum constant name. */
const char *cc_enum_constant_name(EnumConstant *ec);

/*! Get enum constant value. */
int cc_enum_constant_value(EnumConstant *ec);

/*! Get the number of members. Returns -1 if not a struct/union. */
int cc_struct_member_count(JCC *vm, Type *struct_type);

/*! Get member at index (0-based). Returns NULL if out of bounds. */
Member *cc_struct_member_at(JCC *vm, Type *struct_type, int index);

/*! Find member by name. Returns NULL if not found. */
Member *cc_struct_member_find(JCC *vm, Type *struct_type, const char *name);

/*! Get member name. */
const char *cc_member_name(Member *m);

/*! Get member type. */
Type *cc_member_type(Member *m);

/*! Get member offset in bytes. */
int cc_member_offset(Member *m);

/*! Check if member is a bitfield. */
bool cc_member_is_bitfield(Member *m);

/*! Get bitfield width (returns 0 if not a bitfield). */
int cc_member_bitfield_width(Member *m);

/*! Get the TypeKind of a type. */
TypeKind cc_type_kind(Type *ty);

/*! Get sizeof() value in bytes. */
int cc_type_size(Type *ty);

/*! Get alignment in bytes. */
int cc_type_align(Type *ty);

/*! Check if type is unsigned. */
bool cc_type_is_unsigned(Type *ty);

/*! Check if type is const-qualified. */
bool cc_type_is_const(Type *ty);

/*! For pointer/array types: get the base type. Returns NULL if not applicable. */
Type *cc_type_base(Type *ty);

/*! For array types: get the array length. Returns -1 if not an array or is VLA. */
int cc_type_array_len(Type *ty);

/*! For function types: get return type. Returns NULL if not a function. */
Type *cc_type_return_type(Type *ty);

/*! For function types: get parameter count. Returns -1 if not a function. */
int cc_type_param_count(Type *ty);

/*! For function types: get parameter type at index. Returns NULL if out of bounds. */
Type *cc_type_param_at(Type *ty, int index);

/*! For function types: check if variadic. Returns false if not a function. */
bool cc_type_is_variadic(Type *ty);

/*! Get type name (for named types). Returns NULL if unnamed. */
const char *cc_type_name(Type *ty);

/*! Check if type is an integer type. */
bool cc_is_integer(Type *ty);

/*! Check if type is a floating-point type. */
bool cc_is_flonum(Type *ty);

/*! Check if type is a pointer type. */
bool cc_is_pointer(Type *ty);

/*! Check if type is an array type. */
bool cc_is_array(Type *ty);

/*! Check if type is a function type. */
bool cc_is_function(Type *ty);

/*! Check if type is a struct type. */
bool cc_is_struct(Type *ty);

/*! Check if type is a union type. */
bool cc_is_union(Type *ty);

/*! Check if type is an enum type. */
bool cc_is_enum(Type *ty);

/*! Find a global symbol by name. Returns NULL if not found. */
Obj *cc_find_global(JCC *vm, const char *name);

/*! Get the total number of global symbols. */
int cc_global_count(JCC *vm);

/*! Get global symbol at index (0-based). Returns NULL if out of bounds. */
Obj *cc_global_at(JCC *vm, int index);

/*! Get the name of an object (variable or function). */
const char *cc_obj_name(Obj *obj);

/*! Get the type of an object. */
Type *cc_obj_type(Obj *obj);

/*! Check if object is a function. */
bool cc_obj_is_function(Obj *obj);

/*! Check if object is a definition (not just a declaration). */
bool cc_obj_is_definition(Obj *obj);

/*! Check if object has static linkage. */
bool cc_obj_is_static(Obj *obj);

/*! For functions: get parameter count. Returns -1 if not a function. */
int cc_func_param_count(Obj *func);

/*! For functions: get parameter at index. Returns NULL if out of bounds. */
Obj *cc_func_param_at(Obj *func, int index);

/*! For functions: get function body AST. Returns NULL if no body. */
Node *cc_func_body(Obj *func);

/*! Create a numeric literal node. */
Node *cc_node_num(JCC *vm, long long value);

/*! Create a floating-point literal node. */
Node *cc_node_float(JCC *vm, double value);

/*! Create a string literal node. */
Node *cc_node_string(JCC *vm, const char *str);

/*! Create an identifier (variable reference) node. */
Node *cc_node_ident(JCC *vm, const char *name);

/*! Create a binary operation node. */
Node *cc_node_binary(JCC *vm, NodeKind op, Node *left, Node *right);

/*! Create a unary operation node. */
Node *cc_node_unary(JCC *vm, NodeKind op, Node *operand);

/*! Create a function call node. */
Node *cc_node_call(JCC *vm, const char *func_name, Node **args, int arg_count);

/*! Create a member access node (struct.member or ptr->member). */
Node *cc_node_member(JCC *vm, Node *object, const char *member_name);

/*! Create a type cast node. */
Node *cc_node_cast(JCC *vm, Node *expr, Type *target_type);

/*! Create a block (compound statement) node. */
Node *cc_node_block(JCC *vm, Node **stmts, int stmt_count);

/*! Get the NodeKind of a node. */
NodeKind cc_node_kind(Node *node);

/*! Get the type of a node (after type resolution). */
Type *cc_node_type(Node *node);

/*! For numeric literals: get integer value. */
long long cc_node_int_value(Node *node);

/*! For floating-point literals: get value. */
double cc_node_float_value(Node *node);

/*! For string literals: get string content. */
const char *cc_node_string_value(Node *node);

/*! Get left child node (for binary/unary operations). */
Node *cc_node_left(Node *node);

/*! Get right child node (for binary operations). */
Node *cc_node_right(Node *node);

/*! For function calls: get function name. */
const char *cc_node_func_name(Node *node);

/*! For function calls: get argument count. */
int cc_node_arg_count(Node *node);

/*! For function calls: get argument at index. */
Node *cc_node_arg_at(Node *node, int index);

/*! For blocks: get statement count. */
int cc_node_stmt_count(Node *node);

/*! For blocks: get statement at index. */
Node *cc_node_stmt_at(Node *node, int index);

/*! For variable references: get the Obj (variable/function object). */
Obj *cc_node_var(Node *node);

/*! Get the source token for a node (for error reporting). */
Token *cc_node_token(Node *node);

/*! Get the filename from a token. */
const char *cc_token_filename(Token *tok);

/*! Get the line number from a token. */
int cc_token_line(Token *tok);

/*! Get the column number from a token. */
int cc_token_column(Token *tok);

/*! Get the text content of a token. */
int cc_token_text(Token *tok, char *buffer, int bufsize);

/*! Create a pointer type to base. */
Type *cc_make_pointer(Type *base);

/*! Create an array type with specified length. */
Type *cc_make_array(Type *base, int length);

/*! Create a function type. */
Type *cc_make_function(Type *return_type, Type **param_types, int param_count, bool is_variadic);

#ifdef __cplusplus
}
#endif
#endif /* cc_H */
