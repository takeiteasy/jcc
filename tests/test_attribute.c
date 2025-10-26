// Test that __attribute__((...)) is accepted but ignored
// All attributes should be silently ignored by the compiler
// 
// The compiler accepts __attribute__ in all standard positions:
// - Before declarations: __attribute__(...) int x;
// - After type specifiers: int __attribute__(...) x;
// - After declarators: int x __attribute__(...);
// - On struct/union declarations
// - On function parameters
//
// Most attributes are ignored, except:
// - packed: marks struct as packed (no padding)
// - aligned(n): sets alignment to n bytes
//
// Expected return value: 230

// Function attributes
int __attribute__((unused)) unused_func(void) {
    return 42;
}

int __attribute__((noreturn)) noreturn_func(void) {
    return 1; // Should compile even though marked noreturn
}

int __attribute__((const)) const_func(int x) {
    return x * 2;
}

int __attribute__((pure)) pure_func(int x) {
    return x + 1;
}

// Multiple attributes
int __attribute__((unused, const)) multi_attr_func(int x) {
    return x * 3;
}

// Variable attributes
int __attribute__((unused)) unused_var = 10;
int __attribute__((aligned(16))) aligned_var = 20;
int __attribute__((packed)) packed_var = 30;

// Struct attributes
struct __attribute__((packed)) packed_struct {
    char c;
    int i;
};

struct __attribute__((aligned(8))) aligned_struct {
    int x;
    int y;
};

// Type attributes
typedef int __attribute__((aligned(4))) aligned_int;

// Parameter attributes
int param_attr_func(int __attribute__((unused)) x, int y) {
    return y * 2;
}

// GNU-style attributes
int __attribute__((__unused__)) gnu_style(void) {
    return 5;
}

// Complex nested attributes
int __attribute__((nonnull(1,2))) __attribute__((warn_unused_result)) complex_func(int *a, int *b) {
    return *a + *b;
}

int main(void) {
    int result = 0;
    
    // Call functions with attributes
    result += unused_func();           // 42
    result += noreturn_func();         // 1, total 43
    result += const_func(5);           // 10, total 53
    result += pure_func(7);            // 8, total 61
    result += multi_attr_func(3);      // 9, total 70
    
    // Use variables with attributes
    result += unused_var;              // 10, total 80
    result += aligned_var;             // 20, total 100
    result += packed_var;              // 30, total 130
    
    // Test struct attributes
    struct packed_struct ps;
    ps.c = 1;
    ps.i = 5;
    result += ps.i;                    // 5, total 135
    
    struct aligned_struct as;
    as.x = 3;
    as.y = 2;
    result += as.x + as.y;             // 5, total 140
    
    // Test typedef with attribute
    aligned_int ai = 10;
    result += ai;                      // 10, total 150
    
    // Test parameter attributes
    result += param_attr_func(999, 25); // 50, total 200
    
    // Test GNU-style
    result += gnu_style();             // 5, total 205
    
    // Test complex attributes
    int x = 10, y = 15;
    result += complex_func(&x, &y);    // 25, total 230
    
    return result; // Should return 230
}
