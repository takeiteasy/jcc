// Minimal test for struct compound literal with address-of
// Expected return: 42

struct Point {
    int x;
    int y;
};

int main() {
    // Test 1: Struct compound literal assigned to pointer
    struct Point *p1 = &(struct Point){30, 12};
    return 42;
}
