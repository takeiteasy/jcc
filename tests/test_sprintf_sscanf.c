/*
 * Test sprintf, sscanf, and fprintf
 */

#include "stdio.h"

int main() {
    // Test sprintf
    printf("=== Testing sprintf ===\n");
    char buf[100];
    sprintf(buf, "sprintf test: %d", 123);
    printf("Result: %s\n", buf);
    
    sprintf(buf, "x=%d y=%d", 10, 20);
    printf("Result: %s\n", buf);
    
    // Test sscanf
    printf("\n=== Testing sscanf ===\n");
    char *input = "42 99";
    int a, b;
    
    int n = sscanf(input, "%d %d", &a, &b);
    printf("Parsed %d items: a=%d b=%d\n", n, a, b);
    
    // Test simple parsing
    char *num_str = "12345";
    int num;
    sscanf(num_str, "%d", &num);
    printf("Parsed number: %d\n", num);
    
    printf("\n=== All tests passed ===\n");
    return 1;
}
