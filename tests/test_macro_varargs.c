/*
 * Test macro-generated variadic wrappers (0-16 arguments)
 */

#include "stdio.h"

int main() {
    // Test printf with varying argument counts
    printf("Testing 1 arg: %d\n", 42);
    printf("Testing 2 args: %d %d\n", 10, 20);
    printf("Testing 5 args: %d %d %d %d %d\n", 1, 2, 3, 4, 5);
    printf("Testing 10 args: %d %d %d %d %d %d %d %d %d %d\n", 
           1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    printf("Testing 16 args: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", 
           1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    
    // Test sprintf
    char buffer[256];
    sprintf(buffer, "sprintf test: %d %d %d", 100, 200, 300);
    printf("%s\n", buffer);
    
    // Test snprintf with various argument counts
    char buf2[100];
    snprintf(buf2, 100, "snprintf: %d %d %d %d %d", 5, 10, 15, 20, 25);
    printf("%s\n", buf2);

    printf("All macro-generated wrappers working!\n");
    return 42;
}
