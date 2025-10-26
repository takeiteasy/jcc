// Simple struct test
// Expected return: 42

struct Point {
    int x;
    int y;
};

int main() {
    struct Point p;
    p.x = 10;
    p.y = 32;
    
    return p.x + p.y;  // Should return 42
}
