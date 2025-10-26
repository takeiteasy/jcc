// Test function calls
// Expected return: 42

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

int complex_calc(int x) {
    int result = multiply(x, 2);
    result = add(result, 10);
    return result;
}

int main() {
    int x = add(10, 20);        // 30
    int y = multiply(3, 4);     // 12
    int z = complex_calc(16);   // 16*2 + 10 = 42
    return z;
}
