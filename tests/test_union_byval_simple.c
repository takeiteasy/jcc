// Simple union by-value test
// Expected return: 42

union Data {
    int i;
    char c;
};

union Data make_data(int val) {
    union Data d;
    d.i = val;
    return d;
}

int get_value(union Data d) {
    return d.i;
}

int main() {
    union Data d1 = make_data(42);
    if (d1.i != 42) return 1;
    
    int val = get_value(d1);
    if (val != 42) return 2;
    
    return 42;
}
