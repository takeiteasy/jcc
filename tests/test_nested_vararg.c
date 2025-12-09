// Nested variadic test with val*2
#include "stdarg.h"

int inner(int n, ...) {
    va_list args;
    va_start(args, n);
    int a = va_arg(args, int);
    int b = va_arg(args, int);
    va_end(args);
    return a + b;
}

int outer(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int total = 0;
    for (int i = 0; i < count; i++) {
        int val = va_arg(args, int);
        total += inner(2, val, val * 2);  // <-- val*2 instead of val
    }
    
    va_end(args);
    return total;
}

int main() {
    // outer(2, 10, 20)
    // i=0: val=10, inner(2,10,20) = 30
    // i=1: val=20, inner(2,20,40) = 60
    // total = 90
    int result = outer(2, 10, 20);
    if (result != 90) return result;
    return 42;
}
