/*
 * Test: Varargs with struct return
 */

#include "stdarg.h"

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

// Helper: Compare doubles with epsilon
int double_equal(double a, double b, double epsilon) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < epsilon;
}

int main() {
    int a = 5, b = 10;
    
    // sum_all_types(8, 10, 100, 5.5, &a, 20, 200, 10.5, &b)
    AllTypesSums sums = sum_all_types(8, 10, 100, 5.5, &a, 20, 200, 10.5, &b);
    
    if (sums.int_sum != 30) return 1;
    if (sums.long_sum != 300) return 2;
    if (!double_equal(sums.double_sum, 16.0, 0.0001)) return 3;
    if (sums.ptr_sum != 15) return 4;
    
    return 42;
}
