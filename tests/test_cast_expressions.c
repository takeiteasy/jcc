/*
 * Test that cast expressions work correctly in all contexts,
 * including the common (void)expr idiom to mark unused values.
 */

// Helper function to avoid compiler warnings about unused parameters
void use_params(int a, int b) {
    (void)a;  // Common C idiom to mark parameter as intentionally unused
    (void)b;
}

int test_void_casts() {
    int x = 10;
    int y = 20;
    
    // Test (void) cast in expression statement
    (void)x;
    (void)(x + y);
    (void)(x * 2);
    
    return 42;
}

int test_numeric_casts() {
    int x = 10;
    double d = 3.14;
    
    // Test various numeric casts in expression statements
    (int)d;
    (double)x;
    (long)x;
    (char)x;
    
    // Test casts in expressions
    int result = (int)d + x;  // 3 + 10 = 13
    result += (int)(d * 2);   // 13 + 6 = 19
    
    return result;
}

int test_pointer_casts() {
    int x = 42;
    int *p = &x;
    
    // Cast pointer to different types (common in C)
    (void *)p;
    (char *)p;
    (long *)p;
    
    // Use casted pointer
    int value = *(int *)(void *)p;
    return value;  // 42
}

int test_cast_with_side_effects() {
    int x = 5;
    
    // Even though result is cast to void, side effects should happen
    (void)(x++);
    (void)(x += 10);
    
    return x;  // 5 -> 6 -> 16
}

// Test in function call context
int identity(int x) {
    return x;
}

int test_cast_in_call() {
    double d = 7.9;
    return identity((int)d);  // 7
}

int main() {
    int result = 0;
    
    // Call function with unused params (uses (void) casts internally)
    use_params(1, 2);
    
    result += test_void_casts();            // 42
    result += test_numeric_casts();         // 19, total: 61
    result += test_pointer_casts();         // 42, total: 103
    result += test_cast_with_side_effects(); // 16, total: 119
    result += test_cast_in_call();          // 7, total: 126

    if (result != 126) return 1;  // Assert result == 126
    return 42;
}
