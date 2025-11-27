#include <stdio.h>

// Just define, don't use
#define JUST_ARGS(...) __VA_OPT__(__VA_ARGS__)

int main() {
    printf("SUCCESS\n");
    return 42;
}
