# JCC Pragma Macros

JCC introduces a powerful compile-time metaprogramming feature called **Pragma Macros**. This allows you to write C functions that are executed *during compilation* to generate code, inspect types, and perform reflection.

## Overview

A pragma macro is a standard C function prefixed with `#pragma macro`. When JCC encounters this, it:
1. Compiles the function immediately using a separate VM.
2. Exposes the JCC Compiler API to this function.
3. Executes the function when called in your code.
4. Replaces the call with the AST (Abstract Syntax Tree) returned by the macro.

This enables Lisp-like macros in C, allowing you to write functions that write code.

## Basic Usage

To define a pragma macro, simply add `#pragma macro` before a function definition. The function must return a `Node*` (or compatible type depending on what it generates).

```c
#include <stdio.h>

// Note: pragma_api.h is automatically included for pragma macros
// It provides the helper macros used below.

#pragma macro
Node *make_const_5() {
    // Returns the AST node for the integer literal 5
    return AST_INT_LITERAL(5);
}

int main() {
    int x = make_const_5(); // Replaced with: int x = 5;
    printf("%d\n", x);
    return 0;
}
```

## The `__VM` Context

All AST construction and reflection functions require access to the current compiler state (the VM). JCC provides a builtin magic macro `__VM` (wrapping `__jcc_get_vm()`) that provides this context.

The header `pragma_api.h` provides convenience macros (like `AST_INT_LITERAL`) that automatically pass `__VM` for you. You can mostly ignore the `vm` parameter unless you use the low-level `reflection_api.h` directly.

## Reflection API

One of the most powerful features is introspection. You can inspect types defined in your program during compilation.

### Example: Enum to String

This example generates a `to_string` function for any enum automatically.

```c
typedef enum {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE
} Color;

#pragma macro
Node *generate_enum_to_string(char *func_name, char *enum_name) {
    // 1. Find the enum type
    Type *enum_type = AST_FIND_TYPE(enum_name);
    if (!enum_type) return NULL;

    // 2. Create the switch statement
    // switch(val) { ... }
    Node *val_arg = AST_VAR_REF("val");
    Node *switch_stmt = AST_SWITCH(val_arg);

    // 3. Iterate over enum constants and add cases
    int count = AST_ENUM_COUNT(enum_type);
    for (int i = 0; i < count; i++) {
        EnumConstant *ec = AST_ENUM_AT(enum_type, i);
        
        // case ENUM_VAL: return "ENUM_VAL";
        Node *case_val = AST_INT_LITERAL(AST_ENUM_CONSTANT_VALUE(ec));
        Node *return_stmt = AST_RETURN(AST_STRING_LITERAL(AST_ENUM_CONSTANT_NAME(ec)));
        
        AST_SWITCH_ADD_CASE(switch_stmt, case_val, return_stmt);
    }

    // 4. Add default case
    AST_SWITCH_SET_DEFAULT(switch_stmt, 
        AST_RETURN(AST_STRING_LITERAL("UNKNOWN")));

    // 5. Create the function: char *func_name(int val) { ... }
    Type *ret_type = AST_MAKE_POINTER(AST_GET_TYPE("char"));
    Node *func = AST_FUNCTION(func_name, ret_type);
    AST_FUNCTION_ADD_PARAM(func, "val", AST_GET_TYPE("int"));
    AST_FUNCTION_SET_BODY(func, switch_stmt);

    return func;
}

// Generate the function 'color_to_str' for enum 'Color'
generate_enum_to_string("color_to_str", "Color");

int main() {
    printf("Color: %s\n", color_to_str(COLOR_RED)); // Prints "Color: COLOR_RED"
    return 0;
}
```

## API Reference

### Type Introspection

| Macro | Description |
|-------|-------------|
| `AST_FIND_TYPE(name)` | Find a type by name (returns `NULL` if not found) |
| `AST_GET_TYPE(name)` | Get a type (wrapper around find) |
| `AST_TYPE_EXISTS(name)` | Check if a type exists |

### AST Construction

| Macro | Description |
|-------|-------------|
| `AST_INT_LITERAL(val)` | Create integer literal |
| `AST_STRING_LITERAL(str)` | Create string literal |
| `AST_VAR_REF(name)` | Reference a variable |
| `AST_BINARY(op, l, r)` | Binary operation (e.g., `ND_ADD`) |
| `AST_CALL(name, args, n)` | Function call |
| `AST_RETURN(expr)` | Return statement |

### Struct/Enum Reflection

| Macro | Description |
|-------|-------------|
| `AST_ENUM_COUNT(type)` | Number of constants in enum |
| `AST_ENUM_AT(type, i)` | Get enum constant at index |
| `AST_ENUM_CONSTANT_NAME(ec)` | Get name of enum constant |
| `AST_ENUM_CONSTANT_VALUE(ec)` | Get value of enum constant |
| `AST_STRUCT_MEMBER_COUNT(type)` | Number of members in struct |
| `AST_STRUCT_MEMBER_AT(type, i)` | Get member at index |

### Code Generation

| Macro | Description |
|-------|-------------|
| `AST_FUNCTION(name, ret)` | Create new function |
| `AST_FUNCTION_ADD_PARAM(...)` | Add parameter to function |
| `AST_FUNCTION_SET_BODY(...)` | Set function body |
| `AST_SWITCH(cond)` | Create switch statement |
| `AST_SWITCH_ADD_CASE(...)` | Add case to switch |

See `include/pragma_api.h` for the full list of available macros and `include/reflection_api.h` for the underlying API.

```