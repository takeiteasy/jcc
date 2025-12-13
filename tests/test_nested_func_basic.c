/*
 * Test: Basic nested function definition and call
 * Tests simple nested function with no outer variable access
 */

int main() {
    int add(int a, int b) {
        return a + b;
    }
    
    int result = add(40, 2);
    if (result != 42) return 1;
    
    // Test calling nested function multiple times
    if (add(10, 20) != 30) return 2;
    if (add(0, 0) != 0) return 3;
    
    return 42;
}
