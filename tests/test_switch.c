// Test switch statements
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
}

int test_fallthrough(int x) {
    int result = 0;
    switch (x) {
        case 1:
            result = result + 10;
        case 2:
            result = result + 20;
            break;
        case 3:
            result = result + 30;
            break;
        default:
            result = 99;
    }
    return result;
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
    int result = 0;
    
    // Test simple cases
    if (test_simple_switch(1) != 10) return 1;
    if (test_simple_switch(2) != 20) return 2;
    if (test_simple_switch(3) != 30) return 3;
    if (test_simple_switch(5) != 99) return 4;
    
    // Test fallthrough
    if (test_fallthrough(1) != 30) return 5;  // Falls through: 10 + 20
    if (test_fallthrough(2) != 20) return 6;  // Just case 2
    if (test_fallthrough(3) != 30) return 7;  // Just case 3
    if (test_fallthrough(9) != 99) return 8;  // Default
    
    // Test no default
    if (test_no_default(1) != 11) return 9;
    if (test_no_default(2) != 22) return 10;
    if (test_no_default(5) != 33) return 11;  // Falls through
    
    return 42;
}
