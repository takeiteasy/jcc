#include <stdio.h>

#define JUST_ARGS(...) __VA_OPT__(__VA_ARGS__)

int main() {
    int x = 10 JUST_ARGS();
    int y = 10 JUST_ARGS(+ 5);
    printf("x=%d, y=%d\n", x, y);
    return 42;
}
