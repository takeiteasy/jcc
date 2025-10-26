/*
 * Test: Using printf with automatic argument counting
 * 
 * The "stdio.h" header provides a printf() macro that automatically
 * dispatches to printf0-printf10 based on the number of arguments.
 * 
 * Expected output:
 *   Hello, world!
 *   Number: 42
 *   Two numbers: 10 and 20
 *   Three values: 1, 2, 3
 * 
 * Returns: 42 (to verify it ran)
 */

#include "stdio.h"

int main() {
    // Just use printf normally - the macro handles the dispatch!
    printf("Hello, world!\n");                        // → printf1
    printf("Number: %d\n", 42);                       // → printf2
    printf("Two numbers: %d and %d\n", 10, 20);       // → printf3
    printf("Three values: %d, %d, %d\n", 1, 2, 3);    // → printf4
    
    return 42;
}
