// Test combining pragma macros with setjmp/longjmp
#include "setjmp.h"

#pragma macro
long make_error_code(long vm) {
    return ast_int_literal(vm, 42);
}

jmp_buf error_handler;

void function_that_may_error(int should_error) {
    if (should_error) {
        int error_code = make_error_code();
        longjmp(error_handler, error_code);
    }
}

int main() {
    int result = setjmp(error_handler);

    if (result == 0) {
        // First time through
        function_that_may_error(1);  // This will longjmp with error code 42
        return 1;  // Should not reach here
    } else {
        // Returned from longjmp
        if (result == 42) {
            return 0;  // Success! Both features working together
        }
        return 1;  // Wrong error code
    }
}
