/*
 * Test nested designated initializers
 */

struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point top_left;
    struct Point bottom_right;
};

int main() {
    // Test 1: Try the syntax that failed: .top_left.x = value
    struct Rect r1 = {.top_left.x = 10};
    
    if (r1.top_left.x != 10) return 1;
    
    return 42;
}
