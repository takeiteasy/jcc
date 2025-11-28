/*
 JCC: JIT C Compiler - Pragma Macro Convenience API

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
 * @file pragma_api.h
 * @brief Convenience API for JCC Pragma Macros
 *
 * This header provides uppercase convenience macros that automatically
 * pass the VM parameter, making pragma macro code cleaner and easier to write.
 *
 * ## Magic __VM Builtin
 *
 * All convenience macros use the `__VM` macro which expands to __jcc_get_vm(),
 * a builtin function that returns the current VM instance. This works like
 * stdin/stdout/stderr - no parameter passing required!
 *
 * @code
 * #pragma macro
 * long make_5() {
 *     return AST_INT_LITERAL(5);  // __VM is automatically available
 * }
 * @endcode
 *
 * The VM pointer is automatically set before your pragma macro executes and
 * cleared when it returns, so you don't need to pass it as a parameter.
 *
 * ## Usage Example
 *
 * @code
 * // Note: pragma_api.h is automatically included for pragma macros
 *
 * #pragma macro
 * long make_adder(long a, long b) {
 *     // No vm parameter needed! __VM is automatically available
 *     // Create: return a + b;
 *     Node *left = AST_VAR_REF("a");
 *     Node *right = AST_VAR_REF("b");
 *     Node *sum = AST_BINARY(ND_ADD, left, right);
 *     return AST_RETURN(sum);
 * }
 *
 * int main() {
 *     int result = make_adder(3, 5);  // Returns 8
 *     return result;
 * }
 * @endcode
 */

#ifndef PRAGMA_API_H
#define PRAGMA_API_H

typedef struct JCC JCC;
typedef struct Type Type;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Member Member;
typedef struct EnumConstant EnumConstant;
typedef struct Token Token;
// Enum typedefs (should match jcc.h)
typedef enum {
    TY_VOID, TY_BOOL, TY_CHAR, TY_SHORT, TY_INT, TY_LONG,
    TY_FLOAT, TY_DOUBLE, TY_LDOUBLE, TY_ENUM, TY_PTR, TY_FUNC,
    TY_ARRAY, TY_VLA, TY_STRUCT, TY_UNION
} TypeKind;

typedef enum {
    ND_NULL_EXPR, ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_NEG, ND_MOD,
    ND_BITAND, ND_BITOR, ND_BITXOR, ND_SHL, ND_SHR,
    ND_EQ, ND_NE, ND_LT, ND_LE, ND_ASSIGN, ND_COND, ND_COMMA,
    ND_MEMBER, ND_ADDR, ND_DEREF, ND_NOT, ND_BITNOT,
    ND_LOGAND, ND_LOGOR, ND_RETURN, ND_IF, ND_FOR, ND_DO,
    ND_SWITCH, ND_CASE, ND_BLOCK, ND_GOTO, ND_GOTO_EXPR,
    ND_LABEL, ND_LABEL_VAL, ND_FUNCALL, ND_EXPR_STMT,
    ND_STMT_EXPR, ND_VAR, ND_VLA_PTR, ND_NUM, ND_CAST,
    ND_MEMZERO, ND_ASM, ND_CAS, ND_EXCH
} NodeKind;

#include "reflection_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Magic __VM Builtin
// ============================================================================

/*!
 * @function __jcc_get_vm
 * @abstract Builtin function that returns the current parent VM context
 * @discussion This works like stdin/stdout/stderr - it's a function that
 *             returns a pointer to the current VM instance being compiled.
 *             The VM pointer is set before calling a pragma macro and cleared
 *             after it returns. Since pragma macro calls are synchronous and
 *             don't nest, this is thread-safe within a single compilation.
 * @return Pointer to the current JCC VM instance
 */
extern JCC* __jcc_get_vm(void);

/*!
 * @define __VM
 * @abstract Magic macro that references the VM instance in pragma macros
 * @discussion Expands to __jcc_get_vm(), which returns the current parent VM.
 *             This is a builtin similar to stdin/stdout/stderr.
 *             No need to pass vm as a parameter to pragma macro functions!
 */
#define __VM __jcc_get_vm()

// ============================================================================
// Type Lookup and Introspection Macros
// ============================================================================

/*! Find a type by name. Returns NULL if not found. */
#define AST_FIND_TYPE(name) ast_find_type(__VM, name)

/*! Check if a type exists by name. */
#define AST_TYPE_EXISTS(name) ast_type_exists(__VM, name)

/*! Lookup a type by name (high-level wrapper). */
#define AST_GET_TYPE(name) ast_get_type(__VM, name)

// ============================================================================
// Literal Construction Macros
// ============================================================================

/*! Create an integer literal node. */
#define AST_INT_LITERAL(val) ast_int_literal(__VM, val)

/*! Create a string literal node. */
#define AST_STRING_LITERAL(str) ast_string_literal(__VM, str)

/*! Create a variable reference node. */
#define AST_VAR_REF(name) ast_var_ref(__VM, name)

/*! Create a numeric literal node (low-level). */
#define AST_NODE_NUM(val) ast_node_num(__VM, val)

/*! Create a floating-point literal node. */
#define AST_NODE_FLOAT(val) ast_node_float(__VM, val)

/*! Create a string literal node (low-level). */
#define AST_NODE_STRING(str) ast_node_string(__VM, str)

/*! Create an identifier node. */
#define AST_NODE_IDENT(name) ast_node_ident(__VM, name)

// ============================================================================
// Expression Construction Macros
// ============================================================================

/*! Create a binary operation node (e.g., ND_ADD, ND_SUB). */
#define AST_BINARY(op, left, right) ast_node_binary(__VM, op, left, right)

/*! Create a unary operation node (e.g., ND_NEG, ND_NOT). */
#define AST_UNARY(op, operand) ast_node_unary(__VM, op, operand)

/*! Create a function call node. */
#define AST_CALL(func_name, args, arg_count) ast_node_call(__VM, func_name, args, arg_count)

/*! Create a member access node (struct.member or ptr->member). */
#define AST_MEMBER(object, member_name) ast_node_member(__VM, object, member_name)

/*! Create a type cast node. */
#define AST_CAST(expr, target_type) ast_node_cast(__VM, expr, target_type)

// ============================================================================
// Statement Construction Macros
// ============================================================================

/*! Create a block (compound statement) node. */
#define AST_BLOCK(stmts, stmt_count) ast_node_block(__VM, stmts, stmt_count)

/*! Create a return statement node. */
#define AST_RETURN(expr) ast_return(__VM, expr)

/*! Create a switch statement node. */
#define AST_SWITCH(cond) ast_switch(__VM, cond)

/*! Add a case to a switch statement. */
#define AST_SWITCH_ADD_CASE(switch_node, value, body) ast_switch_add_case(__VM, switch_node, value, body)

/*! Set the default case of a switch statement. */
#define AST_SWITCH_SET_DEFAULT(switch_node, body) ast_switch_set_default(__VM, switch_node, body)

// ============================================================================
// Declaration Construction Macros
// ============================================================================

/*! Create a function declaration/definition node. */
#define AST_FUNCTION(name, return_type) ast_function(__VM, name, return_type)

/*! Add a parameter to a function. */
#define AST_FUNCTION_ADD_PARAM(func_node, name, type) ast_function_add_param(__VM, func_node, name, type)

/*! Set the body of a function. */
#define AST_FUNCTION_SET_BODY(func_node, body) ast_function_set_body(__VM, func_node, body)

/*! Create a struct type node. */
#define AST_STRUCT(name) ast_struct(__VM, name)

/*! Add a field to a struct. */
#define AST_STRUCT_ADD_FIELD(struct_node, name, type) ast_struct_add_field(__VM, struct_node, name, type)

// ============================================================================
// Enum Reflection Macros (no VM needed for most)
// ============================================================================

/*! Get the number of enum constants. */
#define AST_ENUM_COUNT(enum_type) ast_enum_count(__VM, enum_type)

/*! Get enum constant at index. */
#define AST_ENUM_AT(enum_type, index) ast_enum_at(__VM, enum_type, index)

/*! Find enum constant by name. */
#define AST_ENUM_FIND(enum_type, name) ast_enum_find(__VM, enum_type, name)

/*! Get the name of an enum type. */
#define AST_ENUM_NAME(e) ast_enum_name(e)

/*! Get the number of values in an enum. */
#define AST_ENUM_VALUE_COUNT(e) ast_enum_value_count(e)

/*! Get the name of an enum value at index. */
#define AST_ENUM_VALUE_NAME(e, index) ast_enum_value_name(e, index)

/*! Get the integer value of an enum constant at index. */
#define AST_ENUM_VALUE(e, index) ast_enum_value(e, index)

/*! Get enum constant name. */
#define AST_ENUM_CONSTANT_NAME(ec) ast_enum_constant_name(ec)

/*! Get enum constant value. */
#define AST_ENUM_CONSTANT_VALUE(ec) ast_enum_constant_value(ec)

// ============================================================================
// Struct/Union Member Introspection Macros
// ============================================================================

/*! Get the number of struct/union members. */
#define AST_STRUCT_MEMBER_COUNT(struct_type) ast_struct_member_count(__VM, struct_type)

/*! Get member at index. */
#define AST_STRUCT_MEMBER_AT(struct_type, index) ast_struct_member_at(__VM, struct_type, index)

/*! Find member by name. */
#define AST_STRUCT_MEMBER_FIND(struct_type, name) ast_struct_member_find(__VM, struct_type, name)

/*! Get member name. */
#define AST_MEMBER_NAME(m) ast_member_name(m)

/*! Get member type. */
#define AST_MEMBER_TYPE(m) ast_member_type(m)

/*! Get member offset. */
#define AST_MEMBER_OFFSET(m) ast_member_offset(m)

/*! Check if member is a bitfield. */
#define AST_MEMBER_IS_BITFIELD(m) ast_member_is_bitfield(m)

/*! Get bitfield width. */
#define AST_MEMBER_BITFIELD_WIDTH(m) ast_member_bitfield_width(m)

// ============================================================================
// Global Symbol Introspection Macros
// ============================================================================

/*! Find a global symbol by name. */
#define AST_FIND_GLOBAL(name) ast_find_global(__VM, name)

/*! Get the total number of global symbols. */
#define AST_GLOBAL_COUNT() ast_global_count(__VM)

/*! Get global symbol at index. */
#define AST_GLOBAL_AT(index) ast_global_at(__VM, index)

// ============================================================================
// Type Construction Macros (no VM needed)
// ============================================================================

/*! Create a pointer type. */
#define AST_MAKE_POINTER(base) ast_make_pointer(_vm, base)

/*! Create an array type. */
#define AST_MAKE_ARRAY(base, length) ast_make_array(_vm, base, length)

/*! Create a function type. */
#define AST_MAKE_FUNCTION(return_type, param_types, param_count, is_variadic) \
    ast_make_function(_vm, return_type, param_types, param_count, is_variadic)

#ifdef __cplusplus
}
#endif

#endif // PRAGMA_API_H
