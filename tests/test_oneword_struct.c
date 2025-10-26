// Minimal test - just check if first byte of struct is copied
struct Point { int x; };

struct Point make_point() {
    struct Point p;
    p.x = 42;
    return p;
}

int main() {
    struct Point p = make_point();
    return p.x;  // Should return 42
}
