// EXPECT_COMPILE_ERROR
// Test that modifying through pointer to const is rejected

int main() {
    int x = 10;
    const int *p = &x;  // Pointer to const int
    *p = 20;            // ERROR: should fail to compile!
    return 0;
}
