// Test pragma macro system - simple make_5 example
// Note: pragma_api.h is automatically included for all pragma macros
// Note: __VM is a builtin (like stdin/stdout) - no vm parameter needed!

#pragma macro
long make_5() {
    return AST_INT_LITERAL(5);
}

int main() {
    int a = make_5();
    if (a != 5) {
        return 1;  // Test failed
    }
    
    return 0;  // Test passed
}

