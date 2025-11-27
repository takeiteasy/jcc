#include <stdio.h>

// Test 1: Basic __VA_OPT__ with comma
#define LOG1(fmt, ...) printf(fmt __VA_OPT__(,) __VA_ARGS__)

int main() {
    // With no variadic args: printf("hello\n")
    LOG1("test1\n");

    // With variadic args: printf("x=%d\n", 42)
    LOG1("test2: x=%d\n", 42);

    printf("SUCCESS\n");
    return 42;
}
