/*
 * Simple test for struct designated initializers: { .member = value }
 * This tests if the parser recognizes the .member = value syntax.
 */

struct Point {
    int x;
    int y;
};

struct Rect {
    int width;
    int height;
    int depth;
};

int main() {
    // Test 1: Basic struct designated initializer
    struct Point p1 = {.x = 10, .y = 20};
    
    if (p1.x != 10) return 1;
    if (p1.y != 20) return 2;
    
    // Test 2: Out-of-order designated initializers
    struct Point p2 = {.y = 30, .x = 40};
    
    if (p2.x != 40) return 3;
    if (p2.y != 30) return 4;
    
    // Test 3: Partial designated initialization (rest should be zero)
    struct Rect r1 = {.width = 100};
    
    if (r1.width != 100) return 5;
    if (r1.height != 0) return 6;  // Should be zero-initialized
    if (r1.depth != 0) return 7;   // Should be zero-initialized
    
    // Test 4: Only last field
    struct Rect r2 = {.depth = 50};
    
    if (r2.width != 0) return 8;   // Should be zero-initialized
    if (r2.height != 0) return 9;  // Should be zero-initialized
    if (r2.depth != 50) return 10;
    
    // Test 5: Mix of designated fields
    struct Rect r3 = {.height = 25, .depth = 75};
    
    if (r3.width != 0) return 11;
    if (r3.height != 25) return 12;
    if (r3.depth != 75) return 13;
    
    // Test 6: Use in expression
    struct Point p3 = {.x = 15, .y = 27};
    int sum = p3.x + p3.y;  // 15 + 27 = 42
    
    if (sum != 42) return 14;
    
    return 42;
}
