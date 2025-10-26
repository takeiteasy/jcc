// Test for designated initializers
// Tests: struct S s = {.x = 1, .y = 2}; and int arr[10] = {[5] = 42};
// Expected return: 42
// NOTE: This feature is likely NOT implemented yet in chibicc

int main() {
    // Test 1: Basic struct designated initializer
    struct Point {
        int x;
        int y;
    };
    
    struct Point p1 = {.x = 10, .y = 20};
    if (p1.x != 10) return 1;
    if (p1.y != 20) return 2;
    
    // Test 2: Out-of-order designated initializers
    struct Point p2 = {.y = 30, .x = 40};
    if (p2.x != 40) return 3;
    if (p2.y != 30) return 4;
    
    // Test 3: Partial designated initialization (rest zero)
    struct Point p3 = {.x = 50};
    if (p3.x != 50) return 5;
    if (p3.y != 0) return 6;  // Should be zero-initialized
    
    // Test 4: Array designated initializer
    int arr1[10] = {[5] = 42};
    if (arr1[5] != 42) return 7;
    if (arr1[0] != 0) return 8;  // Should be zero
    if (arr1[9] != 0) return 9;  // Should be zero
    
    // Test 5: Multiple array designated initializers
    int arr2[6] = {[0] = 1, [2] = 3, [4] = 5};
    if (arr2[0] != 1) return 10;
    if (arr2[1] != 0) return 11;  // Should be zero
    if (arr2[2] != 3) return 12;
    if (arr2[3] != 0) return 13;  // Should be zero
    if (arr2[4] != 5) return 14;
    if (arr2[5] != 0) return 15;  // Should be zero
    
    // Test 6: Nested struct designated initializers
    struct Rect {
        struct Point top_left;
        struct Point bottom_right;
    };
    
    struct Rect r1 = {.top_left.x = 0, .top_left.y = 0, 
                      .bottom_right.x = 100, .bottom_right.y = 100};
    if (r1.top_left.x != 0) return 16;
    if (r1.bottom_right.x != 100) return 17;
    
    // Test 7: Mixed designated and regular initializers
    struct Point p4 = {.x = 5, 10};  // x = 5, y = 10
    if (p4.x != 5) return 18;
    if (p4.y != 10) return 19;
    
    // Test 8: Array with range designated initializer (GNU extension)
    int arr3[8] = {[2 ... 5] = 7};
    if (arr3[0] != 0) return 20;
    if (arr3[2] != 7) return 21;
    if (arr3[3] != 7) return 22;
    if (arr3[4] != 7) return 23;
    if (arr3[5] != 7) return 24;
    if (arr3[6] != 0) return 25;
    
    // Test 9: Designated initializer in expression
    int arr4[5] = {[1] = 10, [3] = 32};
    int result = arr4[1] + arr4[3];  // 10 + 32 = 42
    if (result != 42) return 26;
    
    // Test 10: Complex struct with designated init
    struct Data {
        int id;
        int value;
        int flags;
    };
    
    struct Data d1 = {.value = 42, .id = 1};
    if (d1.id != 1) return 27;
    if (d1.value != 42) return 28;
    if (d1.flags != 0) return 29;  // Should be zero
    
    return d1.value;  // Should return 42
}
