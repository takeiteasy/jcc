/*
 * Test: Incremental varargs double tests
 */

#include "stdarg.h"

// Helper: Compare doubles with epsilon
int double_equal(double a, double b, double epsilon) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < epsilon;
}

// Test 1: Simple double sum
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

// Test 2: Mixed int and double
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

int main() {
    double dresult;
    
    // Test 1: Simple double sum
    dresult = sum_doubles(2, 1.5, 2.5);
    if (!double_equal(dresult, 4.0, 0.0001)) return 1;
    
    // Test 2: More doubles
    dresult = sum_doubles(3, 1.5, 2.5, 3.0);
    if (!double_equal(dresult, 7.0, 0.0001)) return 2;
    
    // Test 3: Mixed
    dresult = mixed_sum(4, 10, 1.5, 20, 2.5);
    if (!double_equal(dresult, 34.0, 0.0001)) return 3;
    
    return 42;
}
