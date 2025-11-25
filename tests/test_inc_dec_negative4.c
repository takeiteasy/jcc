// Test compound assignment with negative
int main() {
    int a = -5;
    a = a + 1;  // Direct assignment

    // Check result (should be 4)
    int result = 0 - a;
    if (result != 4) return 1;
    return 42;
}
