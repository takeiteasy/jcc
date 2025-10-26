/*
 * Test: Struct return with va_arg usage
 */

#include "stdarg.h"

typedef struct {
    int a;
    int b;
} Simple;

Simple use_va_arg(int x, ...) {
    va_list args;
    va_start(args, x);
    
    int y = va_arg(args, int);
    int z = va_arg(args, int);
    
    va_end(args);
    
    Simple result = {x + y, z};
    return result;
}

int main() {
    Simple s = use_va_arg(5, 10, 20);
    if (s.a != 15) return 1;  // 5 + 10
    if (s.b != 20) return 2;
    return 42;
}
