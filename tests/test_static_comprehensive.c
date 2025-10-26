// Comprehensive test for static keyword functionality
// Tests static local variables, static functions, and static globals

// Static global variable (file scope)
static int static_global = 42;

// Static function (file scope)
static int static_helper(int x) {
    return x * 2;
}

// Test 1: Static local variable with initialization
int counter() {
    static int count = 0;
    count = count + 1;
    return count;
}

// Test 2: Multiple static locals in same function
int multi_static() {
    static int a = 10;
    static int b = 20;
    a = a + 1;
    b = b + 2;
    return a + b;  // First call: 11+22=33, Second: 12+24=36, etc.
}

// Test 3: Static local in different functions (separate storage)
int counter_a() {
    static int count = 0;
    count = count + 1;
    return count;
}

int counter_b() {
    static int count = 100;
    count = count + 10;
    return count;
}

// Test 4: Static local with complex initialization
int complex_init() {
    static int value = 5 + 3;  // Should be evaluated once to 8
    value = value + 1;
    return value;
}

// Test 5: Static local without explicit initialization (zero-init)
int zero_init_test() {
    static int uninitialized;
    uninitialized = uninitialized + 1;
    return uninitialized;
}

// Test 6: Static local with array
int array_static() {
    static int arr[3];  // Zero-initialized
    arr[0] = arr[0] + 1;
    arr[1] = arr[1] + 2;
    arr[2] = arr[2] + 3;
    return arr[0] + arr[1] + arr[2];  // First: 1+2+3=6, Second: 2+4+6=12
}

// Test 7: Nested function calls with static locals
int outer() {
    static int x = 0;
    x = x + 1;
    return x + counter();  // counter has its own static
}

int main() {
    // Test static global
    if (static_global != 42) return 1;
    static_global = 50;
    if (static_global != 50) return 2;
    
    // Test static function
    int result = static_helper(5);
    if (result != 10) return 3;
    
    // Test basic counter
    if (counter() != 1) return 4;
    if (counter() != 2) return 5;
    if (counter() != 3) return 6;
    
    // Test multiple statics in one function
    int m1 = multi_static();  // 11 + 22 = 33
    int m2 = multi_static();  // 12 + 24 = 36
    if (m1 != 33) return 7;
    if (m2 != 36) return 8;
    
    // Test separate counters
    if (counter_a() != 1) return 9;
    if (counter_b() != 110) return 10;
    if (counter_a() != 2) return 11;
    if (counter_b() != 120) return 12;
    
    // Test complex initialization
    if (complex_init() != 9) return 13;   // 8 + 1
    if (complex_init() != 10) return 14;  // 9 + 1
    
    // Test zero initialization
    if (zero_init_test() != 1) return 15;
    if (zero_init_test() != 2) return 16;
    
    // Test array static
    if (array_static() != 6) return 17;   // 1+2+3
    if (array_static() != 12) return 18;  // 2+4+6
    
    // Test nested calls
    int o1 = outer();  // x=1, counter()=4, total=5
    int o2 = outer();  // x=2, counter()=5, total=7
    if (o1 != 5) return 19;
    if (o2 != 7) return 20;
    
    // Test static local vs regular local
    int i;
    int regular_sum = 0;
    int static_sum = 0;
    for (i = 0; i < 3; i = i + 1) {
        int regular = 0;
        static int static_var = 0;
        regular = regular + 1;
        static_var = static_var + 1;
        regular_sum = regular_sum + regular;
        static_sum = static_sum + static_var;
    }
    // regular always resets to 0: 1+1+1=3
    // static_var persists: 1+2+3=6
    if (regular_sum != 3) return 21;
    if (static_sum != 6) return 22;
    
    return 42;  // All tests passed!
}
