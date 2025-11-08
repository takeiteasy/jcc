// Test program for enhanced debugger features
// Tests: source mapping, breakpoints by line number, function name breakpoints

int global_var = 100;

int add(int a, int b) {
    int result = a + b;
    return result;
}

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    int result = n * factorial(n - 1);
    return result;
}

int main() {
    int x = 10;
    int y = 20;
    int sum = add(x, y);  // Line 22

    int fact = factorial(5);  // Line 24

    global_var = sum + fact;

    return 0;
}
