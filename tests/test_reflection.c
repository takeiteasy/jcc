/*
 Test JCC Reflection API

 This test verifies the generic reflection API works correctly for:
 - Enum introspection
 - Struct introspection
 - Type queries
 - Symbol lookup
 - AST node creation and inspection
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/internal.h"

// Test enum reflection
void test_enum_reflection() {
    printf("Test: Enum Reflection\n");

    JCC vm;
    cc_init(&vm, false);
    cc_load_stdlib(&vm);

    // Parse code with an enum
    const char *code =
        "enum Color { RED = 1, GREEN = 2, BLUE = 4 };\n";

    Token *tok = cc_preprocess(&vm, NULL);
    tok = tokenize(&vm, new_file("test.c", 1, (char*)code));
    Obj *prog = cc_parse(&vm, tok);

    // Find the Color enum type
    Type *color_enum = cc_find_type(&vm, "Color");
    assert(color_enum != NULL);
    assert(cc_is_enum(color_enum));

    // Test enum count
    int count = cc_enum_count(&vm, color_enum);
    printf("  Enum Color has %d values\n", count);
    assert(count == 3);

    // Test enum access by index
    EnumConstant *ec0 = cc_enum_at(&vm, color_enum, 0);
    assert(ec0 != NULL);
    const char *name0 = cc_enum_constant_name(ec0);
    int val0 = cc_enum_constant_value(ec0);
    printf("  [0] %s = %d\n", name0, val0);
    assert(strcmp(name0, "RED") == 0);
    assert(val0 == 1);

    EnumConstant *ec1 = cc_enum_at(&vm, color_enum, 1);
    assert(ec1 != NULL);
    printf("  [1] %s = %d\n", cc_enum_constant_name(ec1), cc_enum_constant_value(ec1));
    assert(strcmp(cc_enum_constant_name(ec1), "GREEN") == 0);
    assert(cc_enum_constant_value(ec1) == 2);

    // Test enum find by name
    EnumConstant *blue = cc_enum_find(&vm, color_enum, "BLUE");
    assert(blue != NULL);
    printf("  Found BLUE = %d\n", cc_enum_constant_value(blue));
    assert(cc_enum_constant_value(blue) == 4);

    // Test non-existent
    EnumConstant *yellow = cc_enum_find(&vm, color_enum, "YELLOW");
    assert(yellow == NULL);

    cc_destroy(&vm);
    printf("  ✓ Enum reflection test passed\n\n");
}

// Test struct reflection
void test_struct_reflection() {
    printf("Test: Struct Reflection\n");

    JCC vm;
    cc_init(&vm, false);
    cc_load_stdlib(&vm);

    // Parse code with a struct
    const char *code =
        "struct Point {\n"
        "    int x;\n"
        "    int y;\n"
        "    char *name;\n"
        "};\n";

    Token *tok = tokenize(&vm, new_file("test.c", 1, (char*)code));
    Obj *prog = cc_parse(&vm, tok);

    // Find the Point struct type
    Type *point_struct = cc_find_type(&vm, "Point");
    assert(point_struct != NULL);
    assert(cc_is_struct(point_struct));

    // Test member count
    int count = cc_struct_member_count(&vm, point_struct);
    printf("  Struct Point has %d members\n", count);
    assert(count == 3);

    // Test member access by index
    Member *m0 = cc_struct_member_at(&vm, point_struct, 0);
    assert(m0 != NULL);
    const char *name0 = cc_member_name(m0);
    int offset0 = cc_member_offset(m0);
    printf("  [0] %s: offset=%d\n", name0, offset0);
    // Note: name0 might be "x" depending on how cc_member_name extracts the name

    Member *m1 = cc_struct_member_at(&vm, point_struct, 1);
    assert(m1 != NULL);
    printf("  [1] %s: offset=%d\n", cc_member_name(m1), cc_member_offset(m1));

    Member *m2 = cc_struct_member_at(&vm, point_struct, 2);
    assert(m2 != NULL);
    Type *m2_type = cc_member_type(m2);
    assert(m2_type != NULL);
    assert(cc_is_pointer(m2_type));
    printf("  [2] %s: offset=%d (pointer type)\n", cc_member_name(m2), cc_member_offset(m2));

    // Test member find by name
    Member *x_member = cc_struct_member_find(&vm, point_struct, "x");
    assert(x_member != NULL);

    // Test bitfield functions (should return false for regular members)
    assert(!cc_member_is_bitfield(m0));
    assert(cc_member_bitfield_width(m0) == 0);

    cc_destroy(&vm);
    printf("  ✓ Struct reflection test passed\n\n");
}

// Test type queries
void test_type_queries() {
    printf("Test: Type Queries\n");

    JCC vm;
    cc_init(&vm, false);
    cc_load_stdlib(&vm);

    const char *code =
        "typedef int myint;\n"
        "struct Point { int x; int y; };\n"
        "int arr[10];\n"
        "int *ptr;\n"
        "int func(int a, int b);\n";

    Token *tok = tokenize(&vm, new_file("test.c", 1, (char*)code));
    Obj *prog = cc_parse(&vm, tok);

    // Test struct type queries
    Type *point = cc_find_type(&vm, "Point");
    assert(point != NULL);
    printf("  Point: size=%d, align=%d\n", cc_type_size(point), cc_type_align(point));
    assert(cc_type_size(point) > 0);
    assert(cc_is_struct(point));
    assert(!cc_is_union(point));

    // Test array type
    Obj *arr_obj = cc_find_global(&vm, "arr");
    assert(arr_obj != NULL);
    Type *arr_type = cc_obj_type(arr_obj);
    assert(cc_is_array(arr_type));
    assert(cc_type_array_len(arr_type) == 10);
    Type *arr_base = cc_type_base(arr_type);
    assert(arr_base != NULL);
    assert(cc_is_integer(arr_base));
    printf("  arr: array of 10 elements\n");

    // Test pointer type
    Obj *ptr_obj = cc_find_global(&vm, "ptr");
    assert(ptr_obj != NULL);
    Type *ptr_type = cc_obj_type(ptr_obj);
    assert(cc_is_pointer(ptr_type));
    Type *ptr_base = cc_type_base(ptr_type);
    assert(ptr_base != NULL);
    assert(cc_is_integer(ptr_base));
    printf("  ptr: pointer to int\n");

    // Test function type
    Obj *func_obj = cc_find_global(&vm, "func");
    assert(func_obj != NULL);
    assert(cc_obj_is_function(func_obj));
    Type *func_type = cc_obj_type(func_obj);
    assert(cc_is_function(func_type));
    Type *ret_type = cc_type_return_type(func_type);
    assert(ret_type != NULL);
    assert(cc_is_integer(ret_type));
    int param_count = cc_type_param_count(func_type);
    assert(param_count == 2);
    assert(!cc_type_is_variadic(func_type));
    printf("  func: function returning int, %d params\n", param_count);

    cc_destroy(&vm);
    printf("  ✓ Type queries test passed\n\n");
}

// Test symbol lookup
void test_symbol_lookup() {
    printf("Test: Symbol Lookup\n");

    JCC vm;
    cc_init(&vm, false);
    cc_load_stdlib(&vm);

    const char *code =
        "int global_var = 42;\n"
        "static int static_var = 10;\n"
        "int add(int a, int b) { return a + b; }\n";

    Token *tok = tokenize(&vm, new_file("test.c", 1, (char*)code));
    Obj *prog = cc_parse(&vm, tok);

    // Test global count
    int count = cc_global_count(&vm);
    printf("  Total globals: %d\n", count);
    assert(count >= 3); // At least our 3 symbols

    // Test find by name
    Obj *gvar = cc_find_global(&vm, "global_var");
    assert(gvar != NULL);
    assert(!cc_obj_is_function(gvar));
    assert(!cc_obj_is_static(gvar));
    printf("  Found global_var\n");

    Obj *svar = cc_find_global(&vm, "static_var");
    assert(svar != NULL);
    assert(cc_obj_is_static(svar));
    printf("  Found static_var (static)\n");

    Obj *add_func = cc_find_global(&vm, "add");
    assert(add_func != NULL);
    assert(cc_obj_is_function(add_func));
    assert(cc_obj_is_definition(add_func));
    int param_count = cc_func_param_count(add_func);
    assert(param_count == 2);
    printf("  Found function 'add' with %d params\n", param_count);

    // Test param access
    Obj *param0 = cc_func_param_at(add_func, 0);
    assert(param0 != NULL);
    const char *param0_name = cc_obj_name(param0);
    printf("    param[0] = %s\n", param0_name);

    cc_destroy(&vm);
    printf("  ✓ Symbol lookup test passed\n\n");
}

// Test AST node construction and inspection
void test_ast_nodes() {
    printf("Test: AST Node Construction\n");

    JCC vm;
    cc_init(&vm, false);
    cc_load_stdlib(&vm);

    // Create nodes: 3 + 4
    Node *three = cc_node_num(&vm, 3);
    assert(three != NULL);
    assert(cc_node_kind(three) == ND_NUM);
    assert(cc_node_int_value(three) == 3);
    printf("  Created node: 3\n");

    Node *four = cc_node_num(&vm, 4);
    assert(four != NULL);
    assert(cc_node_int_value(four) == 4);
    printf("  Created node: 4\n");

    Node *add = cc_node_binary(&vm, ND_ADD, three, four);
    assert(add != NULL);
    assert(cc_node_kind(add) == ND_ADD);
    Node *left = cc_node_left(add);
    Node *right = cc_node_right(add);
    assert(left == three);
    assert(right == four);
    printf("  Created node: 3 + 4\n");

    // Create float node
    Node *pi = cc_node_float(&vm, 3.14159);
    assert(pi != NULL);
    assert(cc_node_kind(pi) == ND_NUM);
    double val = cc_node_float_value(pi);
    assert(val > 3.14 && val < 3.15);
    printf("  Created node: 3.14159\n");

    cc_destroy(&vm);
    printf("  ✓ AST node construction test passed\n\n");
}

int main() {
    printf("=== JCC Reflection API Test Suite ===\n\n");

    test_enum_reflection();
    test_struct_reflection();
    test_type_queries();
    test_symbol_lookup();
    test_ast_nodes();

    printf("=== All tests passed! ===\n");
    return 0;
}
