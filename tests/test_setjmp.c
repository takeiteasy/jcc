// Test setjmp/longjmp functionality
#include "setjmp.h"
#include "stdio.h"

jmp_buf error_handler;

void function_that_may_fail(int should_fail) {
    if (should_fail) {
        printf("Error occurred! Jumping back...\n");
        longjmp(error_handler, should_fail);
    }
    printf("Function completed successfully\n");
}

int main() {
    int result = setjmp(error_handler);

    if (result == 0) {
        // First time through setjmp
        printf("First call to setjmp, result=%d\n", result);

        // Test successful case
        function_that_may_fail(0);

        // Test error case
        function_that_may_fail(42);

        // This should not be reached
        printf("ERROR: This line should not be reached!\n");
        return 1;
    } else {
        // Returned from longjmp
        printf("Returned from longjmp with value=%d\n", result);

        // Check that we got the right value
        if (result == 42) {
            printf("SUCCESS: setjmp/longjmp working correctly!\n");
            return 0;
        } else {
            printf("ERROR: Expected 42, got %d\n", result);
            return 1;
        }
    }

    return 1;  // Should never reach here
}
