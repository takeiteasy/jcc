// Test switch statements (without break - always return)
// Expected return: 42

int test_simple_switch(int x) {
    switch (x) {
        case 1:
            return 10;
        case 2:
            return 20;
        case 3:
            return 30;
        default:
            return 99;
    }
    return 0;  // Should never reach here
}

int test_no_default(int x) {
    switch (x) {
        case 1:
            return 11;
        case 2:
            return 22;
    }
    return 33;
}

int main() {
    // Test simple cases
    if (test_simple_switch(1) != 10) return 1;
    if (test_simple_switch(2) != 20) return 2;
    if (test_simple_switch(3) != 30) return 3;
    if (test_simple_switch(5) != 99) return 4;
    
    // Test no default
    if (test_no_default(1) != 11) return 5;
    if (test_no_default(2) != 22) return 6;
    if (test_no_default(5) != 33) return 7;  // Falls through to return 33
    
    return 42;
}
