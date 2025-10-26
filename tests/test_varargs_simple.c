/*
 * Simple varargs test - minimal case
 */

#include "stdarg.h"

int sum_two(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int a = va_arg(args, int);
    int b = va_arg(args, int);
    
    va_end(args);
    return a + b;
}

int main() {
    int result = sum_two(2, 10, 20);
    if (result != 30) return 1;
    return 42;
}
