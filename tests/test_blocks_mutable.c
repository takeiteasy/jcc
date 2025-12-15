/*
 * Test: __block storage qualifier for mutable captures
 * Tests that blocks can mutate captured variables with __block
 */

int main() {
    // Test 1: __block variable mutation through block
    __block int x = 10;
    void (^mutate)(int) = ^(int v) { x = v; };
    mutate(42);
    if (x != 42) return 1;
    
    // Test 2: Multiple blocks sharing __block variable
    __block int shared = 0;
    void (^inc)(void) = ^{ shared++; };
    void (^dec)(void) = ^{ shared--; };
    inc(); inc(); dec();
    if (shared != 1) return 2;
    
    // Test 3: __block with block parameters
    __block int counter = 0;
    int (^add_and_get)(int) = ^(int n) { counter += n; return counter; };
    if (add_and_get(5) != 5) return 3;
    if (add_and_get(3) != 8) return 4;
    
    // Test 4: Mix of __block and regular captures
    int const_val = 100;
    __block int mut_val = 0;
    int (^mixed)(void) = ^{ mut_val = const_val; return mut_val; };
    if (mixed() != 100) return 5;
    if (mut_val != 100) return 6;
    
    return 42;
}
