#include <stdio.h>

// Test __VA_OPT__ with token pasting ##
#define PASTE(prefix, ...) prefix __VA_OPT__(##) __VA_ARGS__

int main() {
    int var123 = 42;

    // PASTE(var, 123) should expand to: var ## 123 -> var123
    int a = PASTE(var, 123);
    printf("a=%d (expected 42)\n", a);

    // PASTE(var) should expand to: var (no pasting)
    int var = 100;
    int b = PASTE(var);
    printf("b=%d (expected 100)\n", b);

    if (a == 42 && b == 100) {
        printf("SUCCESS\n");
        return 42;
    }
    return 1;
}
