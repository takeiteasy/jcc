// Test do-while loops
// Expected return value: 42

int test_basic_do_while() {
    int sum = 0;
    int i = 0;
    
    do {
        sum = sum + i;
        i = i + 1;
    } while (i < 5);
    
    // sum = 0 + 1 + 2 + 3 + 4 = 10
    return sum;
}

int test_single_iteration() {
    int x = 0;
    
    do {
        x = 5;
    } while (0);  // Condition is false, but body executes once
    
    return x;  // Should be 5
}

int test_do_while_with_condition() {
    int count = 10;
    int result = 0;
    
    do {
        result = result + count;
        count = count - 1;
    } while (count > 5);
    
    // result = 10 + 9 + 8 + 7 + 6 = 40
    return result;
}

int test_nested_do_while() {
    int outer = 0;
    int inner = 0;
    int total = 0;
    
    do {
        inner = 0;
        do {
            total = total + 1;
            inner = inner + 1;
        } while (inner < 2);
        outer = outer + 1;
    } while (outer < 3);
    
    // Outer loop: 3 iterations
    // Inner loop: 2 iterations each
    // Total: 3 * 2 = 6
    return total;
}

int main() {
    int result = 0;
    
    // Test 1: Basic do-while (should return 10)
    result = test_basic_do_while();
    if (result != 10) {
        return 1;  // Error
    }
    
    // Test 2: Single iteration (should return 5)
    result = test_single_iteration();
    if (result != 5) {
        return 2;  // Error
    }
    
    // Test 3: Do-while with countdown (should return 40)
    result = test_do_while_with_condition();
    if (result != 40) {
        return 3;  // Error
    }
    
    // Test 4: Nested do-while (should return 6)
    result = test_nested_do_while();
    if (result != 6) {
        return 4;  // Error
    }
    
    // All tests passed!
    return 42;
}
