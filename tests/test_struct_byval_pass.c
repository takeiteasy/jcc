// Test struct pass by value
// Expected return: 42

struct Point {
    int x;
    int y;
};

int get_sum(struct Point p) {
    return p.x + p.y;
}

int main() {
    struct Point p;
    p.x = 10;
    p.y = 32;
    return get_sum(p);  // Should be 42
}
