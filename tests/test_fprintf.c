/*
 * Test fprintf functionality
 */

#include "stdio.h"

int main() {
    // Test fprintf to stderr
    fprintf(stderr, "This is an error message\n");
    fprintf(stderr, "Error code: %d\n", 42);
    
    // Test fprintf to stdout
    fprintf(stdout, "This goes to stdout: %d %s\n", 123, "hello");
    
    printf("Test completed\n");
    return 1;
}
