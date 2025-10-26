// Test floating-point function parameters and return values
// Expected return: 42

double add(double a, double b) {
    return a + b;
}

double multiply(double x, double y) {
    return x * y;
}

double subtract(double a, double b) {
    return a - b;
}

int main() {
    // Test 1: Simple function call with float params
    double result = add(20.0, 22.0);
    if (result != 42.0) return 1;
    
    // Test 2: Multiplication
    double prod = multiply(6.0, 7.0);
    if (prod != 42.0) return 2;
    
    // Test 3: Subtraction
    double diff = subtract(50.0, 8.0);
    if (diff != 42.0) return 3;
    
    // Test 4: Nested function calls
    double nested = add(multiply(2.0, 20.0), 2.0);  // (2*20) + 2 = 42
    if (nested != 42.0) return 4;
    
    // Test 5: Variable arguments
    double x = 10.0;
    double y = 32.0;
    double sum = add(x, y);
    if (sum != 42.0) return 5;
    
    return 42;
}
