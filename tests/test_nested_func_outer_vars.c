/*
 * Test: Nested function accessing outer function's local variables
 * Tests static chain access for outer scope variables
 */

int outer_result = 0;

int main() {
    int x = 10;
    int y = 20;
    
    int get_x() {
        return x;  // Access outer's x via static chain
    }
    
    int get_y() {
        return y;  // Access outer's y via static chain
    }
    
    int add_xy() {
        return x + y;  // Access both outer variables
    }
    
    // Test simple access
    if (get_x() != 10) return 1;
    if (get_y() != 20) return 2;
    if (add_xy() != 30) return 3;
    
    // Modify outer variables and verify nested function sees changes
    x = 100;
    if (get_x() != 100) return 4;
    
    y = 200;
    if (get_y() != 200) return 5;
    if (add_xy() != 300) return 6;
    
    return 42;
}
