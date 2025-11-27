#include <stdio.h>

// Simpler test to see what's happening
#define TEST(...) __VA_OPT__(+) __VA_ARGS__

int main() {
    int a = 5 TEST();        // Should be: 5 (no expansion)
    int b = 5 TEST(10);      // Should be: 5 + 10
    int c = 5 TEST(10, 20);  // Should be: 5 + 10, 20 (comma operator)

    printf("a=%d\n", a);
    printf("b=%d\n", b);
    printf("c=%d\n", c);

    return 42;
}
