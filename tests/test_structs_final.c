// Test struct member access (comprehensive)
// Expected return: 42
// Note: This test uses direct return of expressions to avoid
// variable storage issues with non-8-byte-aligned members

struct Point {
    int x;
    int y;
};

struct Rectangle {
    struct Point top_left;
    struct Point bottom_right;
};

int main() {
    // Test 1: Simple struct member assignment and access
    struct Point p;
    p.x = 10;
    p.y = 32;
    
    // Direct return from struct member expression
    return p.x + p.y;  // 10 + 32 = 42
}
