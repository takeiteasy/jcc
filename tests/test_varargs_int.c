/*
 * Test: Variable Argument Lists - Integer-only tests
 */

#include "stdarg.h"

// Test 1: Simple variadic function - sum integers
int sum_ints(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(args, int);
    }
    
    va_end(args);
    return sum;
}

// Test 2: va_copy - copy va_list
int test_va_copy(int count, ...) {
    va_list args1, args2;
    va_start(args1, count);
    va_copy(args2, args1);
    
    // Process with first list
    int sum1 = 0;
    for (int i = 0; i < count; i++) {
        sum1 += va_arg(args1, int);
    }
    
    // Process with copied list (should get same values)
    int sum2 = 0;
    for (int i = 0; i < count; i++) {
        sum2 += va_arg(args2, int);
    }
    
    va_end(args1);
    va_end(args2);
    
    // Both sums should be equal
    return (sum1 == sum2) ? sum1 : -1;
}

// Test 3: Nested variadic calls
int inner_vararg(int n, ...) {
    va_list args;
    va_start(args, n);
    
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += va_arg(args, int);
    }
    
    va_end(args);
    return sum;
}

int outer_vararg(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int total = 0;
    for (int i = 0; i < count; i++) {
        int val = va_arg(args, int);
        // Call another variadic function
        total += inner_vararg(2, val, val * 2);
    }
    
    va_end(args);
    return total;
}

// Test 4: Many arguments (stress test)
int sum_many(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(args, int);
    }
    
    va_end(args);
    return sum;
}

// Test 5: Zero variadic arguments (edge case)
int optional_args(int base, ...) {
    return base;  // Don't use varargs at all
}

// Test 6: Pointer arguments
int sum_via_pointers(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int sum = 0;
    for (int i = 0; i < count; i++) {
        int *ptr = va_arg(args, int*);
        if (ptr) sum += *ptr;
    }
    
    va_end(args);
    return sum;
}

int main() {
    int result = 0;
    
    // Test 1: Simple sum
    // sum_ints(3, 10, 20, 30) = 60
    result = sum_ints(3, 10, 20, 30);
    if (result != 60) return 1;
    
    // Test 2: Different counts
    // sum_ints(5, 1, 2, 3, 4, 5) = 15
    result = sum_ints(5, 1, 2, 3, 4, 5);
    if (result != 15) return 2;
    
    // Test 3: Single argument
    // sum_ints(1, 100) = 100
    result = sum_ints(1, 100);
    if (result != 100) return 3;
    
    // Test 4: va_copy
    // test_va_copy(4, 10, 20, 30, 40) = 100
    result = test_va_copy(4, 10, 20, 30, 40);
    if (result != 100) return 4;
    
    // Test 5: Nested variadic calls
    // outer_vararg(3, 1, 2, 3) = inner_vararg(2,1,2) + inner_vararg(2,2,4) + inner_vararg(2,3,6)
    //                          = 3 + 6 + 9 = 18
    result = outer_vararg(3, 1, 2, 3);
    if (result != 18) return 5;
    
    // Test 6: Many arguments (max 8 total args for register-based calling)
    // sum_many(7, 1, 2, 3, 4, 5, 6, 7) = 28
    result = sum_many(7, 1, 2, 3, 4, 5, 6, 7);
    if (result != 28) return 6;
    
    // Test 7: Zero varargs (edge case)
    // optional_args(42) = 42
    result = optional_args(42);
    if (result != 42) return 7;
    
    // Test 8: Pointer arguments
    int a = 5, b = 10, c = 15;
    // sum_via_pointers(3, &a, &b, &c) = 30
    result = sum_via_pointers(3, &a, &b, &c);
    if (result != 30) return 8;
    
    // Test 9: Large numbers
    // sum_ints(2, 1000, 2000) = 3000
    result = sum_ints(2, 1000, 2000);
    if (result != 3000) return 9;
    
    return 42;  // All tests passed!
}
