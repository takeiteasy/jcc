/*
 * Test: Struct return with double va_arg
 */

#include "stdarg.h"

typedef struct {
    int a;
    double b;
} Mixed;

Mixed use_va_arg_double(int x, ...) {
    va_list args;
    va_start(args, x);
    
    double y = va_arg(args, double);
    double z = va_arg(args, double);
    
    va_end(args);
    
    Mixed result = {x, y + z};
    return result;
}

int main() {
    Mixed m = use_va_arg_double(5, 10.5, 20.5);
    if (m.a != 5) return 1;
    
    // Check if m.b is close to 31.0
    double diff = m.b - 31.0;
    if (diff < 0.0) diff = -diff;
    if (diff > 0.0001) return 2;
    
    return 42;
}
