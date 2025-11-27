#include <stdio.h>

// Test __VA_ARGS__ with single argument
#define ARGS_ONLY(...) __VA_ARGS__
#define WITH_OPT(...) __VA_OPT__(+) __VA_ARGS__

int main() {
    int a = ARGS_ONLY(10);    // Should be: 10
    int b = 5 WITH_OPT(10);   // Should be: 5 + 10 = 15

    printf("a=%d (expected 10)\n", a);
    printf("b=%d (expected 15)\n", b);

    if (a == 10 && b == 15) {
        printf("SUCCESS\n");
        return 42;
    }
    return 1;
}
