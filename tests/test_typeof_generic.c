// Test typeof and _Generic working together
// Demonstrates how these features complement each other for generic programming

// Generic max macro using typeof and _Generic
#define max(a, b) ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _Generic(_a, \
        int: (_a > _b ? _a : _b), \
        long: (_a > _b ? _a : _b), \
        double: (_a > _b ? _a : _b), \
        default: (_a > _b ? _a : _b) \
    ); \
})

// Generic swap macro using typeof
#define swap(a, b) do { \
    typeof(a) _tmp = (a); \
    (a) = (b); \
    (b) = _tmp; \
} while(0)

// Type-safe print function selector using _Generic
int print_int(int x) { return 1; }
int print_long(long x) { return 2; }
int print_double(double x) { return 3; }
int print_char(char x) { return 4; }

#define print(x) _Generic((x), \
    int: print_int, \
    long: print_long, \
    double: print_double, \
    char: print_char \
)(x)

int main() {
    // Test 1: max with ints
    int a = 10, b = 20;
    int result = max(a, b);
    if (result != 20) return 1;
    
    // Test 2: max with doubles
    double dx = 3.14, dy = 2.71;
    double dresult = max(dx, dy);
    if (dresult < 3.13 || dresult > 3.15) return 2;
    
    // Test 3: swap with ints
    int x = 5, y = 15;
    swap(x, y);
    if (x != 15 || y != 5) return 3;
    
    // Test 4: swap with doubles
    double d1 = 1.5, d2 = 2.5;
    swap(d1, d2);
    if (d1 < 2.4 || d1 > 2.6 || d2 < 1.4 || d2 > 1.6) return 4;
    
    // Test 5: Using typeof to declare variables matching function returns
    typeof(max(10, 20)) z = 100;  // z should be int
    if (z != 100) return 5;
    
    // Test 6: Generic print selector
    int print_result = print(42);
    if (print_result != 1) return 6;
    
    print_result = print(42L);
    if (print_result != 2) return 7;
    
    print_result = print(3.14);
    if (print_result != 3) return 8;
    
    // Character literals in C are type int, not char
    // So we need to test with a char variable instead
    char test_char = 'A';
    print_result = print(test_char);
    if (print_result != 4) return 9;
    
    // Test 7: Complex typeof with _Generic
    int i = 5;
    double d = 5.5;
    typeof(_Generic(i > d ? i : d, 
        int: 0, 
        double: 0.0,
        default: 0
    )) mixed_result = 7.5;
    // Result should be double since comparison promotes to double
    if (mixed_result < 7.4 || mixed_result > 7.6) return 10;
    
    // Test 8: typeof preserves type through _Generic
    typeof(_Generic(i, 
        int: 100, 
        default: 200
    )) selected = 150;
    if (selected != 150) return 11;
    
    // Test 9: Array with typeof
    int arr[5] = {1, 2, 3, 4, 5};
    typeof(arr[0]) sum = 0;
    for (typeof(arr[0]) idx = 0; idx < 5; idx++) {
        sum = sum + arr[idx];
    }
    if (sum != 15) return 12;
    
    // Test 10: Pointer with typeof and _Generic
    int value = 999;
    int *ptr = &value;
    typeof(ptr) ptr2;
    ptr2 = ptr;
    if (*ptr2 != 999) return 13;
    
    return 42;  // Success
}
