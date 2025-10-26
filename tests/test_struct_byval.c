// Test struct by-value passing and returning
// Expected return: 42

struct Point {
    int x;
    int y;
};

struct Rectangle {
    struct Point top_left;
    struct Point bottom_right;
};

// Return struct by value
struct Point make_point(int x, int y) {
    struct Point p;
    p.x = x;
    p.y = y;
    return p;
}

// Pass struct by value and return by value
struct Point add_points(struct Point a, struct Point b) {
    struct Point result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

// Pass struct by value, return scalar
int point_sum(struct Point p) {
    return p.x + p.y;
}

// Nested struct by value
struct Rectangle make_rect(int x1, int y1, int x2, int y2) {
    struct Rectangle r;
    r.top_left.x = x1;
    r.top_left.y = y1;
    r.bottom_right.x = x2;
    r.bottom_right.y = y2;
    return r;
}

int rect_area(struct Rectangle r) {
    int width = r.bottom_right.x - r.top_left.x;
    int height = r.bottom_right.y - r.top_left.y;
    return width * height;
}

int main() {
    // Test 1: Return struct by value
    struct Point p1 = make_point(10, 20);
    if (p1.x != 10) return 1;
    if (p1.y != 20) return 2;
    
    // Test 2: Pass and return struct by value
    struct Point p2 = make_point(5, 7);
    struct Point p3 = add_points(p1, p2);
    if (p3.x != 15) return 3;  // 10 + 5
    if (p3.y != 27) return 4;  // 20 + 7
    
    // Test 3: Pass struct by value, return scalar
    int sum = point_sum(p3);
    if (sum != 42) return 5;  // 15 + 27 = 42
    
    // Test 4: Nested structs
    struct Rectangle rect = make_rect(0, 0, 6, 7);
    int area = rect_area(rect);
    if (area != 42) return 6;  // 6 * 7 = 42
    
    // Test 5: Chain function calls
    struct Point p4 = add_points(make_point(1, 2), make_point(3, 4));
    if (p4.x != 4) return 7;
    if (p4.y != 6) return 8;
    
    return 42;
}
