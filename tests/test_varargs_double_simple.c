/*
 * Simple test for double varargs
 */

#include "stdarg.h"

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

int main() {
    // Test: sum_doubles(3, 1.5, 2.5, 3.0) = 7.0
    double result = sum_doubles(3, 1.5, 2.5, 3.0);
    
    // Check if result is close to 7.0
    double diff = result - 7.0;
    if (diff < 0.0) diff = -diff;
    if (diff < 0.0001) {
        return 42;  // Success
    }
    return 1;  // Failure
}
