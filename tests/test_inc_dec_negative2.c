// Test simple ++ with negative
int main() {
    int a = -5;
    int b = a;  // b = -5
    a++;        // a should be -4

    // Check difference
    int result = a - b;  // Should be 1 if increment worked
    if (result != 1) return 1;
    return 42;
}
