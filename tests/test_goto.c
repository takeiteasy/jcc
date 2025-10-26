// Test goto and label statements
// Expected return value: 42

int test_simple_goto() {
    int x = 0;
    goto skip;
    x = 100;  // Should be skipped
skip:
    x = x + 10;
    return x;  // Should return 10
}

int test_backward_goto() {
    int sum = 0;
    int i = 0;
loop:
    sum = sum + i;
    i = i + 1;
    if (i < 5)
        goto loop;
    return sum;  // 0 + 1 + 2 + 3 + 4 = 10
}

int test_forward_goto() {
    int x = 5;
    if (x > 3)
        goto end;
    x = 100;
end:
    return x;  // Should return 5
}

int test_nested_goto() {
    int result = 0;
    int i = 0;
outer:
    if (i >= 3)
        goto done;
    
    int j = 0;
inner:
    if (j >= 2)
        goto next_outer;
    result = result + 1;
    j = j + 1;
    goto inner;

next_outer:
    i = i + 1;
    goto outer;

done:
    return result;  // 3 * 2 = 6
}

int test_goto_over_code() {
    int x = 1;
    goto skip_all;
    
    x = x + 100;
    x = x + 200;
    x = x + 300;
    
skip_all:
    x = x + 2;
    return x;  // Should be 3
}

int main() {
    int result = 0;
    
    // Test 1: Simple forward goto (should return 10)
    result = test_simple_goto();
    if (result != 10) {
        return 1;  // Error
    }
    
    // Test 2: Backward goto (loop simulation, should return 10)
    result = test_backward_goto();
    if (result != 10) {
        return 2;  // Error
    }
    
    // Test 3: Conditional forward goto (should return 5)
    result = test_forward_goto();
    if (result != 5) {
        return 3;  // Error
    }
    
    // Test 4: Nested gotos (should return 6)
    result = test_nested_goto();
    if (result != 6) {
        return 4;  // Error
    }
    
    // Test 5: Goto over multiple statements (should return 3)
    result = test_goto_over_code();
    if (result != 3) {
        return 5;  // Error
    }
    
    // All tests passed!
    return 42;
}
