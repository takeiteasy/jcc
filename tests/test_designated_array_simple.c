/*
 * Simple test for array designated initializers: { [index] = value }
 * This tests if the parser recognizes the [index] = value syntax.
 */

int main() {
    // Test 1: Single designated initializer
    int arr1[5] = {[2] = 42};
    
    if (arr1[0] != 0) return 1;   // Should be zero-initialized
    if (arr1[1] != 0) return 2;   // Should be zero-initialized
    if (arr1[2] != 42) return 3;  // Our designated value
    if (arr1[3] != 0) return 4;   // Should be zero-initialized
    if (arr1[4] != 0) return 5;   // Should be zero-initialized
    
    // Test 2: Multiple designated initializers
    int arr2[6] = {[0] = 10, [2] = 20, [5] = 30};
    
    if (arr2[0] != 10) return 6;
    if (arr2[1] != 0) return 7;
    if (arr2[2] != 20) return 8;
    if (arr2[3] != 0) return 9;
    if (arr2[4] != 0) return 10;
    if (arr2[5] != 30) return 11;
    
    // Test 3: Designated initializer at the end
    int arr3[3] = {[2] = 99};
    
    if (arr3[2] != 99) return 12;
    
    // Test 4: Designated initializer at the beginning
    int arr4[4] = {[0] = 7, [3] = 8};
    
    if (arr4[0] != 7) return 13;
    if (arr4[3] != 8) return 14;
    
    // Test 5: Use in expression
    int arr5[3] = {[1] = 15, [2] = 27};
    int sum = arr5[1] + arr5[2];  // 15 + 27 = 42
    
    if (sum != 42) return 15;
    
    return 42;
}
