// Test local variables in function with params

int test(int a) {
    int x = 10;
    return x;
}

int main() {
    int result = test(99);  // Should return 10, ignoring the param
    if (result != 10) return 1;  // Assert result == 10
    return 42;
}
