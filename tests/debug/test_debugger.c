// Simple test program for the debugger
// Compile with: ./jcc --debug test_debugger.c

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int sum(int a, int b) {
    int result = a + b;
    return result;
}

int main() {
    int x = 5;
    int y = 10;
    int z = sum(x, y);
    int fact = factorial(5);
    return fact;
}
