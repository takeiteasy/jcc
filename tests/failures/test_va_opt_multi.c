#include <stdio.h>

// Test multiple __VA_OPT__ in same macro
#define MULTI(a, ...) a __VA_OPT__(+) __VA_ARGS__ __VA_OPT__(+ 0)

int main() {
    int x = MULTI(5);           // Should be: 5
    int y = MULTI(5, 10);       // Should be: 5 + 10 + 0 = 15
    int z = MULTI(5, 10, 20);   // Should be: 5 + 10 + 20 + 0 = 35

    printf("x=%d (expected 5)\n", x);
    printf("y=%d (expected 15)\n", y);
    printf("z=%d (expected 35)\n", z);

    if (x == 5 && y == 15 && z == 35) {
        printf("SUCCESS\n");
        return 42;
    }
    return 1;
}
