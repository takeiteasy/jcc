// Comprehensive debugger demo program
// Try: ./jcc --debug test_debugger_demo.c
// Or with safety: ./jcc --debug -bounds-checks test_debugger_demo.c

int add(int a, int b) {
    int result = a + b;
    return result;
}

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    int x = 5;
    int y = 10;

    // Simple arithmetic
    int sum = add(x, y);

    // Recursive call
    int fact = factorial(5);

    // Array access
    int arr[5];
    arr[0] = sum;
    arr[1] = fact;

    return arr[0] + arr[1];
}
