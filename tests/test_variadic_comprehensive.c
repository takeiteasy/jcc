/*
 * test_variadic_comprehensive.c - Comprehensive variadic function test
 * Tests printf and sprintf with various argument counts
 */

#include "stdio.h"

int main(void) {
    char buf[256];

    // Test printf with 0-5 variadic args
    printf("=== Testing printf ===\n");
    printf("Test 1\n");
    printf("Test 2: %d\n", 42);
    printf("Test 3: %d %d\n", 10, 20);
    printf("Test 4: %d %d %d\n", 1, 2, 3);
    printf("Test 5: %d %d %d %d\n", 1, 2, 3, 4);
    printf("Test 6: %d %d %d %d %d\n", 1, 2, 3, 4, 5);

    // Test sprintf with 0-3 variadic args
    printf("\n=== Testing sprintf ===\n");
    sprintf(buf, "Hello");
    printf("sprintf result: %s\n", buf);

    sprintf(buf, "Value: %d", 42);
    printf("sprintf result: %s\n", buf);

    sprintf(buf, "Values: %d %d", 10, 20);
    printf("sprintf result: %s\n", buf);

    sprintf(buf, "Values: %d %d %d", 1, 2, 3);
    printf("sprintf result: %s\n", buf);

    // Test with many arguments (10+)
    printf("\n=== Testing many arguments ===\n");
    printf("10 args: %d %d %d %d %d %d %d %d %d %d\n",
           1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

    printf("\n=== All tests passed! ===\n");
    return 0;
}
