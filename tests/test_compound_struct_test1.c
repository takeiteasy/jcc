// Test struct compound literal - test 1 only
// Expected return: 42

struct Point {
    int x;
    int y;
};

int main() {
    // Test 1: Struct compound literal assigned to pointer
    struct Point *p1 = &(struct Point){30, 12};
    int x_val = p1->x;
    int y_val = p1->y;
    int sum = x_val + y_val;
    if (sum != 42) return 1;
    
    return 42;
}
