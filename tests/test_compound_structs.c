// Test struct compound literals with pointer assignment
// Expected return: 42

struct Point {
    int x;
    int y;
};

int main() {
    // Test 1: Struct compound literal assigned to pointer
    struct Point *p1 = &(struct Point){30, 12};
    if (p1->x + p1->y != 42) return 1;
    
    // Test 2: Direct member access on compound literal
    int val = ((struct Point){40, 2}).x + ((struct Point){40, 2}).y;
    if (val != 42) return 2;
    
    // Test 3: Struct compound literal in expression
    struct Point p2 = (struct Point){20, 22};
    if (p2.x + p2.y != 42) return 3;
    
    return 42;
}
