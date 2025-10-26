/*
 * Test: Nested variadic calls with doubles
 */

#include "stdarg.h"

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

int main() {
    // outer_double_call(2, 1.0, 2.0) = inner(2, 1.0, 2.0) + inner(2, 2.0, 4.0)
    //                                 = 3.0 + 6.0 = 9.0
    double result = outer_double_call(2, 1.0, 2.0);
    
    double diff = result - 9.0;
    if (diff < 0.0) diff = -diff;
    if (diff > 0.0001) return 1;
    
    return 42;
}
