// Test floating-point arithmetic operations
// Expected return: 42

int main() {
    // Test 1: Basic float literals and addition
    double x = 10.5;
    double y = 31.5;
    double sum = x + y;  // 42.0
    
    // Test 2: Subtraction
    double a = 50.0;
    double b = 8.0;
    double diff = a - b;  // 42.0
    
    // Test 3: Multiplication
    double m = 6.0;
    double n = 7.0;
    double product = m * n;  // 42.0
    
    // Test 4: Division
    double p = 84.0;
    double q = 2.0;
    double quotient = p / q;  // 42.0
    
    // Test 5: Unary minus
    double neg = -42.0;
    double pos = -neg;  // 42.0
    
    // Test 6: Comparison operators
    double c1 = 42.0;
    double c2 = 42.0;
    double c3 = 41.0;
    
    int eq_test = (c1 == c2);    // 1
    int ne_test = (c1 != c3);    // 1
    int lt_test = (c3 < c1);     // 1
    int le_test = (c3 <= c1);    // 1
    int gt_test = (c1 > c3);     // 1
    int ge_test = (c1 >= c2);    // 1
    
    // Test 7: Type conversion int to float (implicit)
    int int_val = 21;
    double converted = int_val * 2.0;  // 42.0 (int_val implicitly converted)
    
    // Test 8: Complex expression
    double complex_expr = 10.0 * 4.0 + 2.0;  // 42.0
    
    // Test 9: Mixed expressions
    double mixed = 10.0 + 32.0;  // 42.0
    
    // Test 10: Float variable assignment
    double result = 0.0;
    result = 42.0;
    
    // Verify all tests pass and return 42
    if (sum != 42.0) return 1;
    if (diff != 42.0) return 2;
    if (product != 42.0) return 3;
    if (quotient != 42.0) return 4;
    if (pos != 42.0) return 5;
    
    if (!eq_test) return 6;
    if (!ne_test) return 7;
    if (!lt_test) return 8;
    if (!le_test) return 9;
    if (!gt_test) return 10;
    if (!ge_test) return 11;
    
    if (converted != 42.0) return 12;
    if (complex_expr != 42.0) return 13;
    if (mixed != 42.0) return 14;
    if (result != 42.0) return 15;
    
    return 42;
}
