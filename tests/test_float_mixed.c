// Test mixed int and float function parameters
// Expected return: 42

double add_int_float(int i, double d) {
    return i + d;
}

double mixed_ops(double x, int n, double y) {
    return x * n + y;
}

int double_to_int(double d) {
    // Implicit conversion
    return d;
}

int main() {
    // Test 1: Int + Float parameters
    double result1 = add_int_float(10, 32.0);
    if (result1 != 42.0) return 1;
    
    // Test 2: Mixed parameter order
    double result2 = mixed_ops(20.0, 2, 2.0);  // 20*2 + 2 = 42
    if (result2 != 42.0) return 2;
    
    // Test 3: Float to int conversion via function
    int result3 = double_to_int(42.0);
    if (result3 != 42) return 3;
    
    // Test 4: Chained calls
    double result4 = add_int_float(20, add_int_float(10, 12.0));  // 20 + (10 + 12)
    if (result4 != 42.0) return 4;
    
    return 42;
}
