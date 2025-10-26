// Test: Function pointers - basic usage
// Expected return: 42

int add(int a, int b) {
    return a + b;
}

int subtract(int a, int b) {
    return a - b;
}

int multiply(int a, int b) {
    return a * b;
}

int main() {
    // Test 1: Basic function pointer declaration and call
    int (*func_ptr)(int, int);
    
    func_ptr = &add;
    int result1 = func_ptr(10, 20);  // Should be 30
    
    // Test 2: Assign different function
    func_ptr = &subtract;
    int result2 = func_ptr(50, 8);   // Should be 42
    
    // Test 3: Direct assignment without &
    func_ptr = multiply;
    int result3 = func_ptr(6, 7);    // Should be 42
    
    // Return 42 if result2 or result3 is 42
    if (result2 == 42)
        return 42;
    if (result3 == 42)
        return 42;
    
    return result1;  // Shouldn't reach here in passing test
}
