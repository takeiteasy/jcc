// Test advanced union features: pointers, arrays, nested, globals
// Expected return: 42

union Data {
    int i;
    char bytes[8];
};

union Nested {
    int x;
    char a;
};

union Global {
    int val;
    char ch;
} global_union;

int test_pointer() {
    union Data d;
    d.i = 100;
    
    union Data *ptr = &d;
    ptr->i = 200;
    
    return ptr->i == 200 ? 10 : 0;
}

int test_array() {
    union Data d;
    d.i = 0x04030201;  // Write as int
    
    // Read individual bytes (little-endian)
    return d.bytes[0] == 0x01 ? 10 : 0;
}

int test_nested() {
    union Nested n;
    n.a = 10;
    return n.a == 10 ? 10 : 0;
}

int test_global() {
    global_union.val = 42;
    return global_union.val == 42 ? 12 : 0;
}

int main() {
    int result = 0;
    result += test_pointer();
    result += test_array();
    result += test_nested();
    result += test_global();
    return result;  // Should be 42
}
