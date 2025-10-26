/*
 * Test snprintf functionality (safe bounded sprintf)
 */

#include "stdio.h"

int main() {
    char buf[20];  // Small buffer to test bounds
    int n;
    
    // Test basic snprintf
    n = snprintf(buf, 20, "Hello");
    printf("snprintf0: '%s' (returned %d)\n", buf, n);
    
    // Test with argument
    n = snprintf(buf, 20, "x=%d", 42);
    printf("snprintf1: '%s' (returned %d)\n", buf, n);
    
    // Test with multiple arguments
    n = snprintf(buf, 20, "%d+%d=%d", 10, 20, 30);
    printf("snprintf3: '%s' (returned %d)\n", buf, n);
    
    // Test truncation (buffer too small)
    n = snprintf(buf, 10, "This is a very long string");
    printf("Truncated: '%s' (returned %d, needed more space)\n", buf, n);
    
    // Test with zero size (just calculate needed size)
    n = snprintf(buf, 0, "Testing size calculation");
    printf("Size needed: %d bytes\n", n);
    
    printf("All snprintf tests passed!\n");
    return 1;
}
