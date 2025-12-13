/*
 * Test: Nested functions with double arguments and return types
 */

int main() {
    double outer_d = 3.14;

    // Test 1: Nested function with double arguments
    double add_doubles(double a, double b) {
        return a + b;
    }

    if (add_doubles(1.5, 2.5) != 4.0) return 1;

    // Test 2: Nested function accessing outer double
    double add_outer(double v) {
        return v + outer_d;
    }

    if (add_outer(10.0) != 13.14) return 2;

    // Test 3: Mixed arguments (int, double) 
    // This tests if static link (A0) interferes with FP regs
    double mixed(int i, double d) {
        return i + d;
    }

    // i passed in A1 (A0 is s-link), d passed in FA0
    if (mixed(10, 5.5) != 15.5) return 3;

    // Test 4: Verify static link doesn't shift FP args incorrectly
    // mixed2(double a, int b) -> a in FA0, b in A1 (A0 is s-link)
    double mixed2(double a, int b) {
        return a + b;
    }
    
    if (mixed2(10.5, 20) != 30.5) return 4;
    
    // Test 5: Verify outer double variable update
    void update_outer(double val) {
        outer_d = val;
    }
    
    update_outer(99.9);
    if (outer_d != 99.9) return 5;

    return 42;
}
