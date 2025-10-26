// Test switch edge cases
// Expected return: 42

int test_default_only(int x) {
    switch (x) {
        default:
            return 42;
    }
    return 1;
}

int test_single_case(int x) {
    switch (x) {
        case 5:
            return 42;
    }
    return 1;
}

int main() {
    // Test default-only switch
    if (test_default_only(1) != 42) return 1;
    if (test_default_only(999) != 42) return 2;
    
    // Test single case with match
    if (test_single_case(5) != 42) return 3;
    
    // Test single case without match
    if (test_single_case(3) != 1) return 4;
    
    return 42;
}
