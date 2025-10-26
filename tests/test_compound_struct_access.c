// Test struct compound literal with member access
// Expected return: 42

struct Point {
    int x;
    int y;
};

int main() {
    // Test 1: Struct compound literal assigned to pointer
    struct Point *p1 = &(struct Point){30, 12};
    
    // Test 2: Access members through pointer
    int result = p1->x + p1->y;
    
    return result;
}
