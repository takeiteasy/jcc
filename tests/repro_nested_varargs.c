#include "stdio.h"
#include "stdarg.h"

// Simpler inner that doesn't use varargs
int inner_fixed(int a, int b) {
    printf("inner_fixed: a=%d, b=%d\n", a, b);
    return a + b;
}

// Inner that does use varargs
int inner_var(int n, ...) {
    printf("inner_var: n=%d\n", n);
    va_list args;
    va_start(args, n);
    int sum = 0;
    for (int i = 0; i < n; i++) {
        int v = va_arg(args, int);
        printf("inner_var arg[%d] = %d\n", i, v);
        sum += v;
    }
    va_end(args);
    return sum;
}

int outer(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int val1 = va_arg(args, int);
    printf("outer: val1 = %d\n", val1);
    
    // Try calling fixed function first
    int r1 = inner_fixed(val1, val1 * 2);
    printf("outer: inner_fixed returned %d\n", r1);
    
    // Now try calling variadic function
    int r2 = inner_var(2, val1, val1 * 2);
    printf("outer: inner_var returned %d (expected %d)\n", r2, val1 * 3);
    
    va_end(args);
    return r2;
}

int main() {
    int result = outer(1, 10);
    printf("Final result: %d (expected 30)\n", result);
    if (result == 30) return 42;
    return 1;
}
