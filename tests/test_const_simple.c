// Test basic const enforcement

int main() {
    // Test 1: const variable - should compile and work
    const int x = 42;
    int y = x;  // Reading const is OK
    
    // Test should return 42 if const works correctly
    return y;
}
