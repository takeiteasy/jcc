// Comprehensive const test

int global_const = 100;

int main() {
    // Test 1: Basic const variable
    const int x = 10;
    int a = x + 5;  // Reading const is OK
    
    // Test 2: Pointer to const
    int y = 20;
    const int *p1 = &y;
    int val1 = *p1;  // Reading is OK
    
    // Test 3: Const pointer (can modify pointee)
    int z = 30;
    int *const p2 = &z;
    *p2 = 35;
    
    // Test 4: Const pointer to const
    const int *const p3 = &x;
    int val2 = *p3;  // Reading is OK
    
    // Test 5: Global const
    int val3 = global_const;
    
    // Result: 10 + 5 + 20 + 35 + 10 + 100 = 180
    int result = a + val1 + *p2 + val2 + val3;
    if (result != 180) return 1;  // Assert result == 180
    return 42;
}
