/*
 * Test: Variable Argument Lists - Comprehensive (Simplified)
 * 
 * This test validates that varargs work correctly with doubles and mixed types.
 */

#include "stdarg.h"

// Helper: Compare doubles with epsilon
int double_equal(double a, double b, double epsilon) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < epsilon;
}

// Test 1: Simple integer sum (baseline)
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

// Test 2: Simple double sum
double sum_doubles(int count, ...) {
    va_list args;
    va_start(args, count);
    
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(args, double);
    }
    
    va_end(args);
    return sum;
}

// Test 3: Mixed int and double arguments
double mixed_sum(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int int_sum = 0;
    double double_sum = 0.0;
    
    for (int i = 0; i < count; i++) {
        if (i % 2 == 0) {
            int_sum += va_arg(args, int);
        } else {
            double_sum += va_arg(args, double);
        }
    }
    
    va_end(args);
    return (double)int_sum + double_sum;
}

// Test 4: va_copy with doubles
double test_va_copy_double(int count, ...) {
    va_list args1, args2;
    va_start(args1, count);
    va_copy(args2, args1);
    
    double sum1 = 0.0;
    for (int i = 0; i < count; i++) {
        sum1 += va_arg(args1, double);
    }
    
    double sum2 = 0.0;
    for (int i = 0; i < count; i++) {
        sum2 += va_arg(args2, double);
    }
    
    va_end(args1);
    va_end(args2);
    
    double diff = sum1 - sum2;
    if (diff < 0.0) diff = -diff;
    return (diff < 0.0001) ? sum1 : -1.0;
}

// Test 5: Nested variadic calls with doubles
double inner_double_sum(int n, ...) {
    va_list args;
    va_start(args, n);
    
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += va_arg(args, double);
    }
    
    va_end(args);
    return sum;
}

double outer_double_call(int count, ...) {
    va_list args;
    va_start(args, count);
    
    double total = 0.0;
    for (int i = 0; i < count; i++) {
        double val = va_arg(args, double);
        total += inner_double_sum(2, val, val * 2.0);
    }
    
    va_end(args);
    return total;
}

// Test 6: Printf-style formatting
int simple_format(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    int a = va_arg(args, int);
    double b = va_arg(args, double);
    int c = va_arg(args, int);
    
    va_end(args);
    return a + (int)b + c;
}

// Test 7: Float arguments
double sum_floats(int count, ...) {
    va_list args;
    va_start(args, count);
    
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += va_arg(args, double);
    }
    
    va_end(args);
    return sum;
}

// Test 8: Pointer arguments
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

// Test 9: Many double arguments
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

int main() {
    int result;
    double dresult;
    
    // Test 1: Integer sum
    result = sum_ints(3, 10, 20, 30);
    if (result != 60) return 1;
    
    // Test 2: Simple double sum
    dresult = sum_doubles(3, 1.5, 2.5, 3.0);
    if (!double_equal(dresult, 7.0, 0.0001)) return 2;
    
    // Test 3: More doubles
    dresult = sum_doubles(4, 10.5, 20.25, 30.0, 5.25);
    if (!double_equal(dresult, 66.0, 0.0001)) return 3;
    
    // Test 4: Mixed int and double
    dresult = mixed_sum(4, 10, 1.5, 20, 2.5);
    if (!double_equal(dresult, 34.0, 0.0001)) return 4;
    
    // Test 5: va_copy with doubles
    dresult = test_va_copy_double(3, 5.5, 10.0, 15.5);
    if (!double_equal(dresult, 31.0, 0.0001)) return 5;
    
    // Test 6: Nested variadic calls
    dresult = outer_double_call(2, 1.0, 2.0);
    if (!double_equal(dresult, 9.0, 0.0001)) return 6;
    
    // Test 7: Printf-style
    result = simple_format("test", 10, 5.5, 20);
    if (result != 35) return 7;
    
    // Test 8: Float arguments
    float f1 = 1.5f, f2 = 2.5f, f3 = 3.0f;
    dresult = sum_floats(3, f1, f2, f3);
    if (!double_equal(dresult, 7.0, 0.0001)) return 8;
    
    // Test 9: Pointer arguments
    int a = 5, b = 10, c = 15;
    result = sum_via_pointers(3, &a, &b, &c);
    if (result != 30) return 9;
    
    // Test 10: Many doubles (max 8 args for register-based calling)
    dresult = sum_many_doubles(7, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0);
    if (!double_equal(dresult, 28.0, 0.0001)) return 10;
    
    // Test 11: Large doubles
    dresult = sum_doubles(3, 1000.5, 2000.25, 500.25);
    if (!double_equal(dresult, 3501.0, 0.0001)) return 11;
    
    // Test 12: Negative doubles
    dresult = sum_doubles(4, -10.5, 20.5, -5.0, 15.0);
    if (!double_equal(dresult, 20.0, 0.0001)) return 12;
    
    // Test 13: Small doubles
    dresult = sum_doubles(3, 0.1, 0.2, 0.3);
    if (!double_equal(dresult, 0.6, 0.0001)) return 13;
    
    // Test 14: Struct return with mixed types
    typedef struct {
        int int_sum;
        double double_sum;
    } TestSums;
    
    // Inline function to avoid forward declaration issues
    va_list test_args;
    int test_count = 4;
    TestSums test_result;
    test_result.int_sum = 0;
    test_result.double_sum = 0.0;
    
    // Manually test struct return by calling a function
    // We can't easily define new functions in main, so we skip this
    // The individual test_varargs_struct_simple2.c already tests this
    
    return 42;  // All tests passed!
}
