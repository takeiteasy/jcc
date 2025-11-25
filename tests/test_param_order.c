// Debug test - checking parameter order
// Expected: first param should be "a", second should be "b"

int test(int a, int b) {
    // If a=10, b=20, return should be 30
    int sum = a + b;
    return sum;
}

int main() {
    int result = test(10, 20);
    if (result != 30) return 1;  // Assert result == 30
    return 42;
}
