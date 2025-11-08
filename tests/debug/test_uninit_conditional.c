// Test: Conditional initialization bug detection
// Expected: Should detect uninitialized read on else branch

int main() {
    int x;
    int cond = 0;

    if (cond) {
        x = 42;
    }
    // If cond is false, x is never initialized

    return x;  // Should trigger error: reading potentially uninitialized variable
}
