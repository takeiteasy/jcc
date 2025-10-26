/*
 * Test: Float arguments (promoted to double)
 */

#include "stdarg.h"

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

int main() {
    // sum_floats(3, 1.5f, 2.5f, 3.0f) = 7.0
    float f1 = 1.5f, f2 = 2.5f, f3 = 3.0f;
    double result = sum_floats(3, f1, f2, f3);
    
    double diff = result - 7.0;
    if (diff < 0.0) diff = -diff;
    if (diff > 0.0001) return 1;
    
    return 42;
}
