// Minimal struct by-value test
// Expected return: 42

struct Point {
    int x;
    int y;
};

struct Point make_point(int x, int y) {
    struct Point p;
    p.x = x;
    p.y = y;
    return p;
}

int main() {
    struct Point p = make_point(10, 32);
    return p.x + p.y;  // Should be 42
}
