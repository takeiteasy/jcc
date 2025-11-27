#include <stdio.h>

// Test __VA_ARGS__ without __VA_OPT__
#define ARGS_ONLY(...) __VA_ARGS__

int main() {
    int a = ARGS_ONLY(10);      // Should be: 10
    int b = ARGS_ONLY(10, 20);  // Should be: 10, 20 (comma operator, result is 20)

    printf("a=%d (expected 10)\n", a);
    printf("b=%d (expected 20)\n", b);

    if (a == 10 && b == 20) {
        printf("SUCCESS\n");
        return 42;
    }
    return 1;
}
