// Comprehensive function call test
// Tests: multiple functions, parameters, locals, arithmetic, control flow
// Expected return: 42

int square(int x) {
    return x * x;
}

int add(int a, int b) {
    return a + b;
}

int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return add(fibonacci(n - 1), fibonacci(n - 2));
}

int calculate(int x, int y) {
    int sum = add(x, y);
    int sq = square(sum);
    
    if (sq > 100) {
        return sq / 10;
    }
    
    return sq;
}

int main() {
    int a = 5;
    int b = 8;
    int result = calculate(a, b);  // (5+8)^2 / 10 = 169/10 = 16
    
    // Add fibonacci(7) which is 13
    int fib = fibonacci(7);  // 0,1,1,2,3,5,8,13
    
    // Add them together with some extra
    result = add(result, fib);  // 16 + 13 = 29
    result = add(result, square(3));  // 29 + 9 = 38
    result = add(result, 4);  // 38 + 4 = 42
    
    return result;
}
