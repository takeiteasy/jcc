// Test for array partial initialization
// Tests: Partial initialization with remaining elements zero-initialized
// Expected return: 42

int main() {
    // Test 1: Partial initialization - rest should be zero
    int arr1[5] = {1, 2};
    if (arr1[0] != 1) return 1;
    if (arr1[1] != 2) return 2;
    if (arr1[2] != 0) return 3;  // Should be zero-initialized
    if (arr1[3] != 0) return 4;  // Should be zero-initialized
    if (arr1[4] != 0) return 5;  // Should be zero-initialized
    
    // Test 2: Single element initialization
    int arr2[10] = {42};
    if (arr2[0] != 42) return 6;
    if (arr2[1] != 0) return 7;
    if (arr2[9] != 0) return 8;
    
    // Test 3: Empty initialization (all zeros)
    int arr3[5] = {0};
    if (arr3[0] != 0) return 9;
    if (arr3[4] != 0) return 10;
    
    // Test 4: Partial initialization with arithmetic
    int arr4[7] = {10, 20, 30};
    int sum = arr4[0] + arr4[1] + arr4[2];  // 10 + 20 + 30 = 60
    // arr4[3..6] should be 0
    if (sum != 60) return 11;
    if (arr4[3] != 0) return 12;
    if (arr4[6] != 0) return 13;
    
    // Test 5: Access zero-initialized elements
    int arr5[8] = {1, 2, 3};
    int result = 0;
    for (int i = 3; i < 8; i = i + 1) {
        result = result + arr5[i];  // All should be 0
    }
    if (result != 0) return 14;
    
    // Test 6: Partial init with loop access
    int arr6[6] = {5, 10, 15};
    result = 0;
    for (int i = 0; i < 6; i = i + 1) {
        result = result + arr6[i];  // 5 + 10 + 15 + 0 + 0 + 0 = 30
    }
    if (result != 30) return 15;
    
    // Test 7: Global-like pattern (more elements)
    int arr7[20] = {1, 2, 3, 4, 5};
    if (arr7[0] != 1) return 16;
    if (arr7[4] != 5) return 17;
    if (arr7[5] != 0) return 18;
    if (arr7[19] != 0) return 19;
    
    // Test 8: Calculate result from partial array
    int arr8[4] = {10, 32};  // {10, 32, 0, 0}
    result = arr8[0] + arr8[1] + arr8[2] + arr8[3];  // 10 + 32 + 0 + 0 = 42
    
    return result;  // Should return 42
}
