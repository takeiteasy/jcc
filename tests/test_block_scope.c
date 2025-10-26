// Test for block scope and variable shadowing
// Tests: Nested scopes with same variable names
// Expected return: 42

int main() {
    int x = 10;
    int result = 0;
    
    // Test 1: Basic shadowing
    {
        int x = 20;  // Different x - shadows outer x
        result = x;  // Should be 20
    }
    // Outer x should still be 10
    if (x != 10) return 1;
    if (result != 20) return 2;
    
    // Test 2: Multiple levels of nesting
    {
        int x = 30;  // Shadow level 1
        {
            int x = 40;  // Shadow level 2
            result = x;  // Should be 40
        }
        // Back to level 1, should be 30
        if (x != 30) return 3;
    }
    if (result != 40) return 4;
    if (x != 10) return 5;  // Original x still 10
    
    // Test 3: Different variable in nested scope
    {
        int y = 5;
        result = x + y;  // 10 + 5 = 15
    }
    if (result != 15) return 6;
    
    // Test 4: Shadowing with arithmetic
    result = 0;
    {
        int x = 2;
        {
            int x = 3;
            result = x * 10;  // 3 * 10 = 30
        }
        result = result + x;  // 30 + 2 = 32
    }
    result = result + x;  // 32 + 10 = 42
    if (result != 42) return 7;
    
    // Test 5: Multiple variables in same scope
    {
        int a = 1;
        int b = 2;
        int c = 3;
        result = a + b + c;  // 1 + 2 + 3 = 6
    }
    if (result != 6) return 8;
    
    // Test 6: Shadowing in loops
    result = 0;
    for (int i = 0; i < 3; i = i + 1) {
        int x = 100;  // New x in each iteration
        result = result + 1;
    }
    if (result != 3) return 9;
    if (x != 10) return 10;  // Original x unchanged
    
    // Test 7: Shadowing with assignments
    {
        int x = 50;
        x = x + 10;  // 60
        {
            int x = 5;
            x = x + 1;  // 6
            result = x;
        }
        if (x != 60) return 11;  // Middle scope x
    }
    if (result != 6) return 12;
    if (x != 10) return 13;  // Outer x still 10
    
    // Test 8: Complex nesting with control flow
    result = 0;
    {
        int x = 1;
        if (x == 1) {
            int x = 2;
            if (x == 2) {
                int x = 3;
                result = x;  // Should be 3
            }
            result = result + x;  // 3 + 2 = 5
        }
        result = result + x;  // 5 + 1 = 6
    }
    if (result != 6) return 14;
    
    // Test 9: Block scope in while loop
    result = 0;
    int count = 0;
    while (count < 2) {
        int x = 7;
        result = result + x;  // Add 7 each iteration
        count = count + 1;
    }
    if (result != 14) return 15;  // 7 * 2 = 14
    if (x != 10) return 16;  // Original x unchanged
    
    // Test 10: Final complex test
    result = 0;
    {
        int a = 10;
        {
            int a = 20;
            {
                int a = 12;
                result = a;  // 12
            }
            result = result + a;  // 12 + 20 = 32
        }
        result = result + a;  // 32 + 10 = 42
    }
    
    return result;  // Should return 42
}
