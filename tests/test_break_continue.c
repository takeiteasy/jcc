// Test break and continue statements in loops
// Expected return value: 42

int test_break_for() {
    int sum = 0;
    int i = 0;
    for (i = 0; i < 10; i = i + 1) {
        if (i == 5)
            break;
        sum = sum + i;
    }
    return sum;  // 0 + 1 + 2 + 3 + 4 = 10
}

int test_continue_for() {
    int sum = 0;
    int i = 0;
    for (i = 0; i < 10; i = i + 1) {
        if (i == 2 || i == 4)
            continue;
        sum = sum + i;
    }
    return sum;  // 0 + 1 + 3 + 5 + 6 + 7 + 8 + 9 = 39
}

int test_break_while() {
    int sum = 0;
    int i = 0;
    while (i < 10) {
        if (i == 4)
            break;
        sum = sum + i;
        i = i + 1;
    }
    return sum;  // 0 + 1 + 2 + 3 = 6
}

int test_continue_while() {
    int sum = 0;
    int i = 0;
    while (i < 6) {
        i = i + 1;
        if (i == 2 || i == 4)
            continue;
        sum = sum + i;
    }
    return sum;  // 1 + 3 + 5 + 6 = 15
}

int test_break_do_while() {
    int sum = 0;
    int i = 0;
    do {
        if (i == 3)
            break;
        sum = sum + i;
        i = i + 1;
    } while (i < 10);
    return sum;  // 0 + 1 + 2 = 3
}

int test_continue_do_while() {
    int sum = 0;
    int i = 0;
    do {
        i = i + 1;
        if (i == 2)
            continue;
        sum = sum + i;
    } while (i < 4);
    return sum;  // 1 + 3 + 4 = 8
}

int test_nested_break() {
    int result = 0;
    int i = 0;
    for (i = 0; i < 5; i = i + 1) {
        int j = 0;
        for (j = 0; j < 5; j = j + 1) {
            result = result + 1;
            if (j == 2)
                break;  // Break inner loop only
        }
    }
    return result;  // 5 * 3 = 15
}

int test_nested_continue() {
    int result = 0;
    int i = 0;
    for (i = 0; i < 3; i = i + 1) {
        int j = 0;
        for (j = 0; j < 5; j = j + 1) {
            if (j == 2)
                continue;  // Skip j=2 in inner loop
            result = result + 1;
        }
    }
    return result;  // 3 * 4 = 12
}

int main() {
    int result = 0;
    
    // Test 1: Break in for loop (should return 10)
    result = test_break_for();
    if (result != 10) {
        return 1;  // Error
    }
    
    // Test 2: Continue in for loop (should return 39)
    result = test_continue_for();
    if (result != 39) {
        return 2;  // Error
    }
    
    // Test 3: Break in while loop (should return 6)
    result = test_break_while();
    if (result != 6) {
        return 3;  // Error
    }
    
    // Test 4: Continue in while loop (should return 15)
    result = test_continue_while();
    if (result != 15) {
        return 4;  // Error
    }
    
    // Test 5: Break in do-while loop (should return 3)
    result = test_break_do_while();
    if (result != 3) {
        return 5;  // Error
    }
    
    // Test 6: Continue in do-while loop (should return 8)
    result = test_continue_do_while();
    if (result != 8) {
        return 6;  // Error
    }
    
    // Test 7: Nested break (should return 15)
    result = test_nested_break();
    if (result != 15) {
        return 7;  // Error
    }
    
    // Test 8: Nested continue (should return 12)
    result = test_nested_continue();
    if (result != 12) {
        return 8;  // Error
    }
    
    // All tests passed!
    return 42;
}
