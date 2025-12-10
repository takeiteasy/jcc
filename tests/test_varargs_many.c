/*
 * Test: Variadic functions with more than 8 total arguments
 * 
 * This test verifies that variadic functions correctly handle
 * calls with 9+ total arguments (exceeding the 8-register limit).
 * Arguments 1-8 are passed in registers and spilled to bp[-1...-8].
 * Arguments 9+ are passed on the stack at bp[+2...].
 */

#include "stdarg.h"
#include "stdio.h"

// Helper: compare doubles with epsilon
int double_equal(double a, double b, double epsilon) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < epsilon;
}

// Test 1: Sum many integers (11 total args)
int sum_many_ints(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(args, int);
    }
    
    va_end(args);
    return sum;
}

// Test 2: Sum many doubles (11 total args)  
double sum_many_doubles(int count, ...) {
    va_list args;
    va_start(args, count);
    
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(args, double);
    }
    
    va_end(args);
    return sum;
}

// Test 3: Mixed int and double with many args
// Format: int, double, int, double, ...
double sum_mixed_many(int count, ...) {
    va_list args;
    va_start(args, count);
    
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        if (i % 2 == 0) {
            sum += va_arg(args, int);
        } else {
            sum += va_arg(args, double);
        }
    }
    
    va_end(args);
    return sum;
}

int main() {
    int iresult;
    double dresult;
    
    // Test 1: 11 total args (1 count + 10 int varargs)
    // Expected: 1+2+3+4+5+6+7+8+9+10 = 55
    iresult = sum_many_ints(10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    printf("sum_many_ints(10, 1..10) = %d (expected 55)\n", iresult);
    if (iresult != 55) return 1;
    
    // Test 2: 11 total args (1 count + 10 double varargs)
    // Expected: 1.0+2.0+...+10.0 = 55.0
    dresult = sum_many_doubles(10, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);
    printf("sum_many_doubles(10, 1.0..10.0) = %f (expected 55.0)\n", dresult);
    if (!double_equal(dresult, 55.0, 0.0001)) return 2;
    
    // Test 3: 12 total args (1 count + 11 mixed varargs)
    // 0, 1.5, 2, 3.5, 4, 5.5, 6, 7.5, 8, 9.5, 10 = 57.5
    dresult = sum_mixed_many(11, 0, 1.5, 2, 3.5, 4, 5.5, 6, 7.5, 8, 9.5, 10);
    printf("sum_mixed_many(11, 0..10) = %f (expected 57.5)\n", dresult);
    if (!double_equal(dresult, 57.5, 0.0001)) return 3;
    
    printf("All many-arg varargs tests passed!\n");
    return 42;
}
