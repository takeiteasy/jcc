/*
 * test_system_include.c - Test system include paths with <...>
 * This test uses <stdio.h> instead of "stdio.h" to verify
 * that system include paths work correctly.
 */

#include <stdio.h>

int main(void) {
    printf("Testing system includes: <stdio.h>\n");
    printf("Value: %d\n", 42);
    printf("Multiple values: %d, %d, %d\n", 1, 2, 3);
    return 0;
}
