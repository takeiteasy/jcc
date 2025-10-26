// Nested struct test
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
    struct Rectangle r;
    r.top_left.x = 5;
    r.top_left.y = 15;
    r.bottom_right.x = 20;
    r.bottom_right.y = 2;
    
    // 5 + 15 + 20 + 2 = 42
    return r.top_left.x + r.top_left.y + r.bottom_right.x + r.bottom_right.y;
}
