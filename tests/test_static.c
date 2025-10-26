// Test static keyword functionality
// Tests:
// 1. Static local variables (retain value between function calls)
// 2. Static global variables (file scope only - not testable in single file)
// 3. Static functions (file scope only - not testable in single file)

// Test 1: Static local variable
int counter() {
    static int count = 0;  // Initialized once, persists between calls
    count = count + 1;
    return count;
}

// Test 2: Static local with explicit initialization
int sum_values(int x) {
    static int total = 100;  // Initialized to 100 on first call
    total = total + x;
    return total;
}

// Test 3: Static local without initialization (should be zero-initialized)
int zero_init() {
    static int value;  // Should be 0
    value = value + 10;
    return value;
}

int main() {
    int result = 0;
    
    // Test counter - should return 1, 2, 3
    int c1 = counter();  // Should be 1
    int c2 = counter();  // Should be 2
    int c3 = counter();  // Should be 3
    
    if (c1 != 1) return 1;
    if (c2 != 2) return 2;
    if (c3 != 3) return 3;
    
    // Test sum_values - should start at 100
    int s1 = sum_values(5);   // Should be 105
    int s2 = sum_values(10);  // Should be 115
    
    if (s1 != 105) return 4;
    if (s2 != 115) return 5;
    
    // Test zero initialization
    int z1 = zero_init();  // Should be 10 (0 + 10)
    int z2 = zero_init();  // Should be 20 (10 + 10)
    
    if (z1 != 10) return 6;
    if (z2 != 20) return 7;
    
    return 42;  // All tests passed
}
