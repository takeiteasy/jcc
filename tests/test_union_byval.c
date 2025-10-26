// Test union by-value passing and returning
// Expected return: 42

union Data {
    int i;
    char c;
    long l;
};

struct Pair {
    int first;
    int second;
};

union Mixed {
    int i;
    struct Pair p;
};

// Return union by value
union Data make_data_int(int value) {
    union Data d;
    d.i = value;
    return d;
}

// Pass union by value and return by value
union Data copy_data(union Data d) {
    union Data result;
    result.i = d.i;
    return result;
}

// Pass union by value, return scalar
int get_int_from_data(union Data d) {
    return d.i;
}

// Union with struct member
union Mixed make_mixed(int first, int second) {
    union Mixed m;
    m.p.first = first;
    m.p.second = second;
    return m;
}

int main() {
    // Test 1: Return union by value
    union Data d1 = make_data_int(42);
    if (d1.i != 42) return 1;
    
    // Test 2: Pass and return union by value
    union Data d2 = copy_data(d1);
    if (d2.i != 42) return 2;
    
    // Test 3: Pass union by value, return scalar
    int val = get_int_from_data(d2);
    if (val != 42) return 3;
    
    // Test 4: Union with struct member
    union Mixed mixed = make_mixed(10, 32);
    if (mixed.p.first != 10) return 4;
    if (mixed.p.second != 32) return 5;
    
    // Test 5: Modify union member
    union Data d3;
    d3.i = 100;
    d3 = make_data_int(42);
    if (d3.i != 42) return 6;
    
    return 42;
}
