// Test: Uninitialized variable detection
// This should be caught by -z/--uninitialized-detection flag
// Expected: Error when reading uninitialized variable

int main() {
    int x;  // Uninitialized
    return x;  // Should trigger error: reading uninitialized variable
}
