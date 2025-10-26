// Comprehensive comma operator test
// Tests comma expressions in various contexts
// Expected return: 42

int main() {
    int x = 0;
    int y = 0;
    int result = 0;
    
    // Test 1: Basic comma in expression (value comes from right side)
    result = (x = 5, y = 10, x + y);  // result = 15
    
    // Test 2: Comma with side effects
    int a = 0;
    int b = (a = 3, a * 2);  // b = 6, a = 3
    
    // Test 3: Comma in condition
    if ((x = 7, y = 8, x < y)) {
        result = result + 10;  // 15 + 10 = 25
    }
    
    // Test 4: Comma in for loop init
    int i = 0;
    int sum = 0;
    for (i = 0, sum = 0; i < 3; i = i + 1) {
        sum = sum + 1;
    }
    result = result + sum;  // 25 + 3 = 28
    
    // Test 5: Nested comma expressions
    int c = (a = 1, (b = 2, a + b));  // c = 3
    
    // Test 6: Comma in function argument context
    // Note: In function calls, commas separate arguments, not comma operator
    // But in parenthesized expressions within arguments, comma operator works
    result = result + (a = 5, b = 6, a + b);  // 28 + 11 = 39
    
    // Test 7: Multiple comma operators in sequence
    int final = (x = 1, y = 2, x + y);  // final = 3
    
    // Final calculation: result + final
    // = 39 + 3 = 42
    return result + final;
}
