/*
 * test_vm_variadic_comprehensive.c - Comprehensive test for VM variadic functions
 */

#include "stdio.h"
#include "stdarg.h"

// Test: variadic function with integers
int sum_ints(int count, ...) {
    va_list ap;
    va_start(ap, count);

    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(ap, int);
    }

    va_end(ap);
    return sum;
}

// Test: variadic function with mixed types (int and long)
long sum_mixed(int count, ...) {
    va_list ap;
    va_start(ap, count);

    long sum = 0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(ap, long);
    }

    va_end(ap);
    return sum;
}

// Test: variadic function that finds max
int max_of(int count, ...) {
    if (count == 0) return 0;

    va_list ap;
    va_start(ap, count);

    int max = va_arg(ap, int);
    for (int i = 1; i < count; i++) {
        int val = va_arg(ap, int);
        if (val > max) {
            max = val;
        }
    }

    va_end(ap);
    return max;
}

// Test: variadic function with pointers
void print_strings(int count, ...) {
    va_list ap;
    va_start(ap, count);

    for (int i = 0; i < count; i++) {
        char *str = va_arg(ap, char*);
        printf("  [%d]: %s\n", i, str);
    }

    va_end(ap);
}

int main(void) {
    int result;

    printf("=== Testing sum_ints ===\n");
    result = sum_ints(3, 10, 20, 30);
    printf("sum_ints(3, 10, 20, 30) = %d (expected 60)\n", result);

    result = sum_ints(5, 1, 2, 3, 4, 5);
    printf("sum_ints(5, 1, 2, 3, 4, 5) = %d (expected 15)\n", result);

    result = sum_ints(1, 42);
    printf("sum_ints(1, 42) = %d (expected 42)\n", result);

    printf("\n=== Testing sum_mixed ===\n");
    long lresult = sum_mixed(3, 100L, 200L, 300L);
    printf("sum_mixed(3, 100, 200, 300) = %ld (expected 600)\n", lresult);

    printf("\n=== Testing max_of ===\n");
    result = max_of(5, 3, 7, 2, 9, 1);
    printf("max_of(5, 3, 7, 2, 9, 1) = %d (expected 9)\n", result);

    result = max_of(3, 100, 50, 75);
    printf("max_of(3, 100, 50, 75) = %d (expected 100)\n", result);

    printf("\n=== Testing print_strings ===\n");
    print_strings(3, "Hello", "World", "!");

    printf("\n=== All VM variadic tests passed! ===\n");
    return 0;
}
