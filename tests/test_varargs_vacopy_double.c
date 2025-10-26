/*
 * Test: va_copy with doubles
 */

#include "stdarg.h"

double test_va_copy_double(int count, ...) {
    va_list args1, args2;
    va_start(args1, count);
    va_copy(args2, args1);
    
    // Process with first list
    double sum1 = 0.0;
    for (int i = 0; i < count; i++) {
        sum1 += va_arg(args1, double);
    }
    
    // Process with copied list
    double sum2 = 0.0;
    for (int i = 0; i < count; i++) {
        sum2 += va_arg(args2, double);
    }
    
    va_end(args1);
    va_end(args2);
    
    // Check if equal
    double diff = sum1 - sum2;
    if (diff < 0.0) diff = -diff;
    if (diff < 0.0001) {
        return sum1;
    } else {
        return -1.0;
    }
}

int main() {
    double result = test_va_copy_double(3, 5.5, 10.0, 15.5);
    
    // Should be 31.0
    double diff = result - 31.0;
    if (diff < 0.0) diff = -diff;
    if (diff > 0.0001) return 1;
    
    return 42;
}
