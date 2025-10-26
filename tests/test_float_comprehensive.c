// Floating-point operations test
// Tests arithmetic, comparisons, and mixed operations
// Expected return: 42

double add(double a, double b) {
    return a + b;
}

double multiply(double a, double b) {
    return a * b;
}

int main() {
    // Test 1: Function calls with floats
    double x = add(10.0, 32.0);
    if (x != 42.0) return 1;
    
    // Test 2: Multiplication
    double y = multiply(6.0, 7.0);
    if (y != 42.0) return 2;
    
    // Test 3: Division
    double z = 84.0 / 2.0;
    if (z != 42.0) return 3;
    
    // Test 4: Subtraction
    double w = 50.0 - 8.0;
    if (w != 42.0) return 4;
    
    // Test 5: Complex expression
    double complex = (10.0 + 2.0) * 3.0 + 6.0;  // (12 * 3) + 6 = 42
    if (complex != 42.0) return 5;
    
    // Test 6: Comparison operators
    double a = 42.0;
    double b = 41.0;
    
    if (!(a > b)) return 6;
    if (!(a >= b)) return 7;
    if (!(b < a)) return 8;
    if (!(b <= a)) return 9;
    if (!(a == 42.0)) return 10;
    if (a != 42.0) return 11;
    
    // Test 7: Unary minus
    double neg = -42.0;
    if (neg != -42.0) return 12;
    double pos = -neg;
    if (pos != 42.0) return 13;
    
    // Test 8: Assignment chain
    double val1, val2, val3;
    val1 = val2 = val3 = 42.0;
    if (val1 != 42.0 || val2 != 42.0 || val3 != 42.0) return 14;
    
    // Test 9: Global floats (if supported)
    // For now, just test locals
    
    return 42;
}
