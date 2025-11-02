/*
 Simple test for JCC Reflection API
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/internal.h"

int main() {
    printf("=== JCC Reflection API Simple Test ===\n");

    JCC vm;
    cc_init(&vm, false);
    cc_load_stdlib(&vm);
    printf("VM initialized\n");

    // Parse code with an enum
    const char *code =
        "enum Color { RED, GREEN, BLUE };\n"
        "int main() { return 0; }\n";

    Token *tok = tokenize(&vm, new_file("test.c", 1, (char*)code));
    printf("Code tokenized\n");

    Obj *prog = cc_parse(&vm, tok);
    (void)prog; // Avoid unused warning
    printf("Code parsed\n");

    // Find the Color enum type
    Type *color_enum = ast_find_type(&vm, "Color");
    printf("Finding type 'Color': %p\n", (void*)color_enum);

    if (color_enum) {
        printf("Found Color enum\n");
        printf("Type kind: %d (should be %d for TY_ENUM)\n", ast_type_kind(color_enum), TY_ENUM);

        // Test enum count
        int count = ast_enum_count(&vm, color_enum);
        printf("Enum count: %d\n", count);

        if (count > 0) {
            // Test first enum value
            EnumConstant *ec0 = ast_enum_at(&vm, color_enum, 0);
            if (ec0) {
                const char *name = ast_enum_constant_name(ec0);
                int value = ast_enum_constant_value(ec0);
                printf("Enum[0]: %s = %d\n", name, value);
            }
        }
    } else {
        printf("Color enum not found!\n");
    }

    cc_destroy(&vm);
    printf("=== Test complete ===\n");
    return 0;
}
