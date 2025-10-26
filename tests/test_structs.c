// Test struct member access
// Expected return: 42

struct Point {
    int x;
    int y;
};

struct Rectangle {
    struct Point top_left;
    struct Point bottom_right;
};

int main() {
    // Test 1: Simple struct member access
    struct Point p;
    p.x = 10;
    p.y = 32;
    
    int result = p.x + p.y;  // 10 + 32 = 42
    
    // Test 2: Nested struct access
    struct Rectangle r;
    r.top_left.x = 5;
    r.top_left.y = 15;
    r.bottom_right.x = 20;
    r.bottom_right.y = 2;
    
    // Verify nested access works
    int check = r.top_left.x + r.top_left.y + r.bottom_right.x + r.bottom_right.y;
    // 5 + 15 + 20 + 2 = 42
    
    if (check != 42) return 1;
    
    return result;  // Should return 42
}
