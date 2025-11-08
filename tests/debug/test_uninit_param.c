// Test: Function parameters should be pre-initialized
// Expected: No error, returns 42

int add(int a, int b) {
    return a + b;  // Parameters should be marked as initialized
}

int main() {
    return add(40, 2);
}
