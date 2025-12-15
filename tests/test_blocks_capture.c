/*
 * Test: Block variable capture (capture-by-copy)
 * Tests that blocks capture outer variable values at creation time
 */

int main() {
    // Test 1: Simple capture - block should capture value of x at creation time
    int x = 10;
    int (^get_x)(void) = ^{ return x; };
    
    // Modify x after block creation
    x = 20;
    
    // Block should still return 10 (captured at creation time)
    int result1 = get_x();
    if (result1 != 10) return 1;  // Expected: 10, not 20
    
    // Test 2: Capture multiple variables
    int a = 5;
    int b = 7;
    int (^add_captured)(void) = ^{ return a + b; };
    
    // Modify after capture
    a = 100;
    b = 200;
    
    // Should still return 5 + 7 = 12
    int result2 = add_captured();
    if (result2 != 12) return 2;
    
    // Test 3: Captured variable used with block parameter
    int multiplier = 3;
    int (^multiply)(int) = ^(int n) { return n * multiplier; };
    
    multiplier = 100;  // Won't affect the block
    
    int result3 = multiply(4);  // 4 * 3 = 12
    if (result3 != 12) return 3;
    
    return 42;
}
