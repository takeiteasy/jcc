/*!
 * @file pragma_api.h
 * @brief AST Builder API for JCC Pragma Macros
 *
 * This header provides a comprehensive API for building Abstract Syntax Trees
 * programmatically within #pragma macro functions. It includes functions for:
 * - Type introspection and lookup
 * - Enum reflection
 * - Struct/union introspection
 * - AST node construction (literals, expressions, statements)
 * - Control flow builders (switch, return, if, while, for)
 * - Function and struct construction
 *
 * All functions that accept a JCC* parameter expect the first parameter to be
 * the VM instance passed to your pragma macro function.
 *
 * @example
 * @code
 * #pragma macro
 * long make_int_literal(long vm, long value) {
 *     return ast_int_literal(vm, value);
 * }
 * @endcode
 */

#ifndef PRAGMA_API_H
#define PRAGMA_API_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for opaque types
// Note: These types are defined in the JCC internal headers
// We only need to forward-declare them for the function signatures
typedef long JCC;
typedef long Type;
typedef long Node;
typedef long Obj;
typedef long Member;
typedef long EnumConstant;
typedef long Token;

// Type Lookup and Introspection
// ============================================================================

/*!
 * @function ast_get_type
 * @abstract Find a type by name (high-level wrapper)
 * @param vm The JCC VM instance
 * @param name Name of the type to find (e.g., "int", "struct Point")
 * @return Type pointer or NULL if not found
 */
Type *ast_get_type(JCC *vm, const char *name);

/*!
 * @function ast_find_type
 * @abstract Find a type by name in the current scope
 * @param vm The JCC VM instance
 * @param name Name of the type to find
 * @return Type pointer or NULL if not found
 */
Type *ast_find_type(JCC *vm, const char *name);

/*!
 * @function ast_type_exists
 * @abstract Check if a type exists by name
 * @param vm The JCC VM instance
 * @param name Type name to check
 * @return true if type exists, false otherwise
 */
long ast_type_exists(JCC *vm, const char *name);

// High-Level Literal Constructors
// ============================================================================

/*!
 * @function ast_int_literal
 * @abstract Create an integer literal node
 * @param vm The JCC VM instance
 * @param value Integer value
 * @return Node representing the integer literal
 */
Node *ast_int_literal(JCC *vm, long long value);

/*!
 * @function ast_string_literal
 * @abstract Create a string literal node
 * @param vm The JCC VM instance
 * @param str String value
 * @return Node representing the string literal
 */
Node *ast_string_literal(JCC *vm, const char *str);

/*!
 * @function ast_var_ref
 * @abstract Create a variable reference node
 * @param vm The JCC VM instance
 * @param name Variable name
 * @return Node representing the variable reference
 */
Node *ast_var_ref(JCC *vm, const char *name);

// Enum Reflection (High-Level API)
// ============================================================================

/*!
 * @function ast_enum_name
 * @abstract Get the name of an enum type
 * @param e Enum type
 * @return Enum name or NULL
 */
const char *ast_enum_name(Type *e);

/*!
 * @function ast_enum_value_count
 * @abstract Get the number of values in an enum
 * @param e Enum type
 * @return Number of enum values
 */
unsigned long ast_enum_value_count(Type *e);

/*!
 * @function ast_enum_value_name
 * @abstract Get the name of an enum value at index
 * @param e Enum type
 * @param index Index of the enum value (0-based)
 * @return Name of the enum value or NULL
 */
const char *ast_enum_value_name(Type *e, unsigned long index);

/*!
 * @function ast_enum_value
 * @abstract Get the integer value of an enum constant at index
 * @param e Enum type
 * @param index Index of the enum value (0-based)
 * @return Integer value of the enum constant
 */
long long ast_enum_value(Type *e, unsigned long index);

// Control Flow Builders
// ============================================================================

/*!
 * @function ast_switch
 * @abstract Create a switch statement node
 * @param vm The JCC VM instance
 * @param condition Expression to switch on
 * @return Node representing the switch statement
 */
Node *ast_switch(JCC *vm, Node *condition);

/*!
 * @function ast_switch_add_case
 * @abstract Add a case to a switch statement
 * @param vm The JCC VM instance
 * @param switch_node The switch statement node
 * @param value Case value (integer literal node)
 * @param body Case body (statement node)
 */
void ast_switch_add_case(JCC *vm, Node *switch_node, Node *value, Node *body);

/*!
 * @function ast_switch_set_default
 * @abstract Set the default case of a switch statement
 * @param vm The JCC VM instance
 * @param switch_node The switch statement node
 * @param body Default case body (statement node)
 */
void ast_switch_set_default(JCC *vm, Node *switch_node, Node *body);

/*!
 * @function ast_return
 * @abstract Create a return statement node
 * @param vm The JCC VM instance
 * @param expr Expression to return (or NULL for void return)
 * @return Node representing the return statement
 */
Node *ast_return(JCC *vm, Node *expr);

// Function Construction
// ============================================================================

/*!
 * @function ast_function
 * @abstract Create a function definition node
 * @param vm The JCC VM instance
 * @param name Function name
 * @param return_type Function return type
 * @return Node representing the function
 */
Node *ast_function(JCC *vm, const char *name, Type *return_type);

/*!
 * @function ast_function_add_param
 * @abstract Add a parameter to a function
 * @param vm The JCC VM instance
 * @param func_node Function node
 * @param name Parameter name
 * @param type Parameter type
 */
void ast_function_add_param(JCC *vm, Node *func_node, const char *name, Type *type);

/*!
 * @function ast_function_set_body
 * @abstract Set the body of a function
 * @param vm The JCC VM instance
 * @param func_node Function node
 * @param body Function body (statement node)
 */
void ast_function_set_body(JCC *vm, Node *func_node, Node *body);

// Struct Construction (Note: Implementation incomplete - use with caution)
// ============================================================================

/*!
 * @function ast_struct
 * @abstract Create a struct type (placeholder implementation)
 * @param vm The JCC VM instance
 * @param name Struct name
 * @return Node representing the struct type
 * @note Implementation is incomplete
 */
Node *ast_struct(JCC *vm, const char *name);

/*!
 * @function ast_struct_add_field
 * @abstract Add a field to a struct (placeholder implementation)
 * @param vm The JCC VM instance
 * @param struct_node Struct node
 * @param name Field name
 * @param type Field type
 * @note Implementation is incomplete
 */
void ast_struct_add_field(JCC *vm, Node *struct_node, const char *name, Type *type);

#ifdef __cplusplus
}
#endif

#endif // PRAGMA_API_H
