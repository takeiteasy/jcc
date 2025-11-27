#include <stdio.h>

// Try with something before ##
#define PASTE2(prefix, ...) prefix __VA_OPT__(## __VA_ARGS__)

int main() {
    int var123 = 42;
    int a = PASTE2(var, 123);
    printf("a=%d (expected 42)\n", a);

    int var = 100;
    int b = PASTE2(var);
    printf("b=%d (expected 100)\n", b);

    return (a == 42 && b == 100) ? 42 : 1;
}
