// Test struct member values
// If a=100 and b=200, return 42
// Otherwise return error code

struct Test {
    int a;
    int b;
};

int main() {
    struct Test t;
    t.a = 100;
    t.b = 200;
    
    int a_val = t.a;
    int b_val = t.b;
    
    if (a_val != 100) return 1;  // a is wrong
    if (b_val != 200) return 2;  // b is wrong
    
    return 42;  // Both correct
}
