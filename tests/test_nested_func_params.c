/*
 * Test: Nested function accessing outer function's parameters
 * Tests static chain access for parameters (which are local to the function)
 */

int test_params(int a, int b) {
    int sum() {
        return a + b;  // Access outer's parameters
    }
    
    int diff() {
        return a - b;  // Access outer's parameters
    }
    
    if (sum() != a + b) return 1;
    if (diff() != a - b) return 2;
    
    return 0;
}

int main() {
    if (test_params(30, 12) != 0) return 1;  // Tests sum=42, diff=18
    if (test_params(100, 50) != 0) return 2; // Tests sum=150, diff=50
    if (test_params(5, 5) != 0) return 3;    // Tests sum=10, diff=0
    
    return 42;
}
