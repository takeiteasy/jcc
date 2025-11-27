#include <stdio.h>

#define JUST_ARGS(...) __VA_OPT__(__VA_ARGS__)

int main() {
    int x = 10 JUST_ARGS();
    printf("x=%d\n", x);
    return 42;
}
