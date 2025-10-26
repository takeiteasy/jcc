/*
 * Test: Simple struct with varargs
 */

#include "stdarg.h"

typedef struct {
    int a;
    int b;
} Simple;

Simple make_simple(int x, ...) {
    Simple result = {x, x * 2};
    return result;
}

int main() {
    Simple s = make_simple(5, 10, 20);
    if (s.a != 5) return 1;
    if (s.b != 10) return 2;
    return 42;
}
