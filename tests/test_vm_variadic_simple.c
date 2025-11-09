/*
 * test_vm_variadic_simple.c - Test VM-compiled variadic functions
 */

#include "stdio.h"
#include "stdarg.h"

// User-defined variadic function compiled to VM bytecode
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

int main(void) {
    int result;

    // Test with 3 arguments
    result = sum_ints(3, 10, 20, 30);
    printf("sum_ints(3, 10, 20, 30) = %d (expected 60)\n", result);

    // Test with 5 arguments
    result = sum_ints(5, 1, 2, 3, 4, 5);
    printf("sum_ints(5, 1, 2, 3, 4, 5) = %d (expected 15)\n", result);

    return 42;
}
