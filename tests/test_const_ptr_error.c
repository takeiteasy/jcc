// Test that changing const pointer is rejected

int main() {
    int x = 10;
    int y = 20;
    int *const p = &x;  // Const pointer to int
    p = &y;             // ERROR: should fail to compile!
    return 0;
}
