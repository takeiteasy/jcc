// Test both pass and return
// Expected return: 42

struct Point {
    int x;
    int y;
};

struct Point add_points(struct Point a, struct Point b) {
    struct Point result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

int main() {
    struct Point p1;
    p1.x = 10;
    p1.y = 20;
    
    struct Point p2;
    p2.x = 5;
    p2.y = 7;
    
    struct Point p3 = add_points(p1, p2);
    
    return p3.x + p3.y;  // Should be 42 (15 + 27)
}
