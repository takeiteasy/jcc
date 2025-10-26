/*
 * Test: Varargs with multiple types (simplified struct)
 */

#include "stdarg.h"

typedef struct {
    int int_sum;
    double double_sum;
} SimpleSums;

SimpleSums sum_simple_types(int count, ...) {
    va_list args;
    va_start(args, count);
    
    SimpleSums result;
    result.int_sum = 0;
    result.double_sum = 0.0;
    
    for (int i = 0; i < count; i++) {
        int type = i % 2;
        if (type == 0) {
            result.int_sum += va_arg(args, int);
        } else {
            result.double_sum += va_arg(args, double);
        }
    }
    
    va_end(args);
    return result;
}

int double_equal(double a, double b, double epsilon) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < epsilon;
}

int main() {
    // sum_simple_types(4, 10, 5.5, 20, 10.5)
    //   int: 10, 20 = 30
    //   double: 5.5, 10.5 = 16.0
    SimpleSums sums = sum_simple_types(4, 10, 5.5, 20, 10.5);
    
    if (sums.int_sum != 30) return 1;
    if (!double_equal(sums.double_sum, 16.0, 0.0001)) return 2;
    
    return 42;
}
