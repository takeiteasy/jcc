// Test pragma macro system - simple make_5 example

#pragma macro
long make_5(long vm) {
    return ast_int_literal(vm, 5);
}

int main() {
    int a = make_5();
    if (a != 5) {
        return 1;  // Test failed
    }
    
    return 0;  // Test passed
}

