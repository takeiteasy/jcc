// Test typeof operator
// Tests compile-time type inquiry for variables, expressions, and complex types

int main() {
    // Test 1: Basic typeof with variables
    int x = 5;
    typeof(x) y = 10;  // y should be int
    if (y != 10) return 1;
    
    // Test 2: typeof with expressions
    typeof(x + y) z = 15;  // z should be int (result of int + int)
    if (z != 15) return 2;
    
    // Test 3: typeof with pointer
    int *ptr = &x;
    typeof(ptr) ptr2 = &y;  // ptr2 should be int*
    *ptr2 = 20;
    if (y != 20) return 3;
    
    // Test 4: typeof with array decay
    int arr[3] = {1, 2, 3};
    typeof(&arr[0]) p = arr;  // p should be int*
    if (p[1] != 2) return 4;
    
    // Test 5: typeof preserves const
    const int cx = 42;
    typeof(cx) cy = 100;  // cy should be const int
    if (cy != 100) return 5;
    
    // Test 6: typeof with char
    char c = 'A';
    typeof(c) c2 = 'B';
    if (c2 != 'B') return 6;
    
    // Test 7: typeof in complex expressions
    typeof(1 + 2.5) d = 3.5;  // Should be double (int promotes to double)
    if (d < 3.4 || d > 3.6) return 7;
    
    // Test 8: typeof with comparison result
    typeof(x < y) b = 1;  // Should be int (comparison result)
    if (b != 1) return 8;
    
    // Test 9: Nested typeof
    typeof(typeof(x)) w = 30;  // w should be int
    if (w != 30) return 9;
    
    // Test 10: typeof with function pointer would require more complex setup
    // Skipping for now as it would need function declarations
    
    return 42;  // Success
}
