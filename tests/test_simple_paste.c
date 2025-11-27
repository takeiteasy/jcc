#include <stdio.h>

// Simpler test - just ## outside __VA_OPT__
#define SIMPLE_PASTE(a, b) a ## b

int main() {
    int var123 = 42;
    int x = SIMPLE_PASTE(var, 123);
    printf("x=%d (expected 42)\n", x);
    return x == 42 ? 42 : 1;
}
