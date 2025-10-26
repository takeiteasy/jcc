// Test 3 only: Struct compound literal in expression
// Expected return: 42

struct Point {
    int x;
    int y;
};

int main() {
    // Test 3: Struct compound literal in expression
    struct Point p2 = (struct Point){20, 22};
    if (p2.x + p2.y != 42) return 3;
    
    return 42;
}
