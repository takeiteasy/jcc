/*
 * Test: Variable Argument Lists - Comprehensive (including doubles)
 * 
 * This test validates that varargs work correctly with:
 * 1. Integer arguments (existing functionality)
 * 2. Double/float arguments (newly implemented)
 * 3. Mixed int and double arguments
 * 4. va_copy with doubles
 * 5. Nested variadic calls with doubles
 * 6. Printf-style formatting with mixed types
 */

#include "stdarg.h"

// =============================================================================
// Test 1: Simple integer sum (baseline - should still work)
// =============================================================================
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

// =============================================================================
// Test 2: Simple double sum (new functionality)
// =============================================================================
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

// =============================================================================
// Test 3: Mixed int and double arguments
// =============================================================================
// Format: alternating int, double, int, double, ...
// Returns sum of integers + sum of doubles
double mixed_sum(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int int_sum = 0;
    double double_sum = 0.0;
    
    for (int i = 0; i < count; i++) {
        if (i % 2 == 0) {
            // Even indices: integer
            int_sum += va_arg(args, int);
        } else {
            // Odd indices: double
            double_sum += va_arg(args, double);
        }
    }
    
    va_end(args);
    return (double)int_sum + double_sum;
}

// =============================================================================
// Test 4: va_copy with doubles
// =============================================================================
double test_va_copy_double(int count, ...) {
    va_list args1, args2;
    va_start(args1, count);
    va_copy(args2, args1);
    
    // Process with first list
    double sum1 = 0.0;
    for (int i = 0; i < count; i++) {
        sum1 += va_arg(args1, double);
    }
    
    // Process with copied list (should get same values)
    double sum2 = 0.0;
    for (int i = 0; i < count; i++) {
        sum2 += va_arg(args2, double);
    }
    
    va_end(args1);
    va_end(args2);
    
    // Both sums should be equal
    // Return sum1 if equal, otherwise return -1.0
    // Use small epsilon for floating-point comparison
    double diff = sum1 - sum2;
    if (diff < 0.0) diff = -diff;
    if (diff < 0.0001) {
        return sum1;
    } else {
        return -1.0;
    }
}

// =============================================================================
// Test 5: Nested variadic calls with doubles
// =============================================================================
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
        // Call another variadic function with double
        total += inner_double_sum(2, val, val * 2.0);
    }
    
    va_end(args);
    return total;
}

// =============================================================================
// Test 6: Printf-style formatting (simplified - just formatting, no printing)
// =============================================================================
// Returns: int_val + (int)double_val + int_val2
// This simulates a printf("%d %.2f %d", int_val, double_val, int_val2)
int simple_format(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    // Read three arguments: int, double, int
    int a = va_arg(args, int);
    double b = va_arg(args, double);
    int c = va_arg(args, int);
    
    va_end(args);
    
    // Return a + (int)b + c for testing
    return a + (int)b + c;
}

// =============================================================================
// Test 7: Float arguments (should be promoted to double)
// =============================================================================
double sum_floats(int count, ...) {
    va_list args;
    va_start(args, count);
    
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        // Note: float arguments are promoted to double in varargs
        sum += va_arg(args, double);
    }
    
    va_end(args);
    return sum;
}

// =============================================================================
// Test 8: All types mixed (int, long, double, pointer)
// =============================================================================
typedef struct {
    int int_sum;
    long long_sum;
    double double_sum;
    int ptr_sum;
} AllTypesSums;

AllTypesSums sum_all_types(int count, ...) {
    va_list args;
    va_start(args, count);
    
    AllTypesSums result = {0, 0, 0.0, 0};
    
    for (int i = 0; i < count; i++) {
        int type = i % 4;
        if (type == 0) {
            result.int_sum += va_arg(args, int);
        } else if (type == 1) {
            result.long_sum += va_arg(args, long);
        } else if (type == 2) {
            result.double_sum += va_arg(args, double);
        } else {
            int *ptr = va_arg(args, int*);
            if (ptr) result.ptr_sum += *ptr;
        }
    }
    
    va_end(args);
    return result;
}

// =============================================================================
// Test 9: Zero variadic arguments with double return type
// =============================================================================
double optional_double(int base, ...) {
    return (double)base;  // Don't use varargs at all
}

// =============================================================================
// Test 10: Many double arguments (stress test)
// =============================================================================
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

// =============================================================================
// Helper: Compare doubles with epsilon
// =============================================================================
int double_equal(double a, double b, double epsilon) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < epsilon;
}

// =============================================================================
// Main test runner
// =============================================================================
int main() {
    int result;
    double dresult;
    
    // Test 1: Integer sum (baseline - existing functionality)
    result = sum_ints(3, 10, 20, 30);
    if (result != 60) return 1;
    
    // Test 2: Simple double sum
    // sum_doubles(3, 1.5, 2.5, 3.0) = 7.0
    dresult = sum_doubles(3, 1.5, 2.5, 3.0);
    if (!double_equal(dresult, 7.0, 0.0001)) return 2;
    
    // Test 3: More doubles
    // sum_doubles(4, 10.5, 20.25, 30.0, 5.25) = 66.0
    dresult = sum_doubles(4, 10.5, 20.25, 30.0, 5.25);
    if (!double_equal(dresult, 66.0, 0.0001)) return 3;
    
    // Test 4: Mixed int and double
    // mixed_sum(4, 10, 1.5, 20, 2.5) = (10+20) + (1.5+2.5) = 30 + 4.0 = 34.0
    dresult = mixed_sum(4, 10, 1.5, 20, 2.5);
    if (!double_equal(dresult, 34.0, 0.0001)) return 4;
    
    // Test 5: va_copy with doubles
    // test_va_copy_double(3, 5.5, 10.0, 15.5) = 31.0 (if both sums match)
    dresult = test_va_copy_double(3, 5.5, 10.0, 15.5);
    if (!double_equal(dresult, 31.0, 0.0001)) return 5;
    
    // Test 6: Nested variadic calls with doubles
    // outer_double_call(2, 1.0, 2.0) = inner(2, 1.0, 2.0) + inner(2, 2.0, 4.0)
    //                                 = 3.0 + 6.0 = 9.0
    dresult = outer_double_call(2, 1.0, 2.0);
    if (!double_equal(dresult, 9.0, 0.0001)) return 6;
    
    // Test 7: Printf-style formatting
    // simple_format("", 10, 5.5, 20) = 10 + 5 + 20 = 35
    result = simple_format("test", 10, 5.5, 20);
    if (result != 35) return 7;
    
    // Test 8: Float arguments (promoted to double)
    // sum_floats(3, 1.5f, 2.5f, 3.0f) = 7.0
    float f1 = 1.5f, f2 = 2.5f, f3 = 3.0f;
    dresult = sum_floats(3, f1, f2, f3);
    if (!double_equal(dresult, 7.0, 0.0001)) return 8;
    
    // Test 9: All types mixed
    int a = 5, b = 10, c = 15;
    // sum_all_types(8, 10, 100, 5.5, &a, 20, 200, 10.5, &b)
    //   int: 10, 20 = 30
    //   long: 100, 200 = 300
    //   double: 5.5, 10.5 = 16.0
    //   ptr: &a=5, &b=10 = 15
    AllTypesSums sums = sum_all_types(8, 10, 100, 5.5, &a, 20, 200, 10.5, &b);
    if (sums.int_sum != 30) return 9;
    if (sums.long_sum != 300) return 10;
    if (!double_equal(sums.double_sum, 16.0, 0.0001)) return 11;
    if (sums.ptr_sum != 15) return 12;
    
    // Test 10: Zero varargs with double return
    dresult = optional_double(42);
    if (!double_equal(dresult, 42.0, 0.0001)) return 13;
    
    // Test 11: Many double arguments (stress test)
    // sum_many_doubles(10, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0) = 55.0
    dresult = sum_many_doubles(10, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);
    if (!double_equal(dresult, 55.0, 0.0001)) return 14;
    
    // Test 12: Large double values
    // sum_doubles(3, 1000.5, 2000.25, 500.25) = 3501.0
    dresult = sum_doubles(3, 1000.5, 2000.25, 500.25);
    if (!double_equal(dresult, 3501.0, 0.0001)) return 15;
    
    // Test 13: Negative doubles
    // sum_doubles(4, -10.5, 20.5, -5.0, 15.0) = 20.0
    dresult = sum_doubles(4, -10.5, 20.5, -5.0, 15.0);
    if (!double_equal(dresult, 20.0, 0.0001)) return 16;
    
    // Test 14: Very small doubles
    // sum_doubles(3, 0.1, 0.2, 0.3) = 0.6
    dresult = sum_doubles(3, 0.1, 0.2, 0.3);
    if (!double_equal(dresult, 0.6, 0.0001)) return 17;
    
    return 42;  // All tests passed!
}
