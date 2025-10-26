// Test union return - absolute minimal
union Data { int i; };

union Data make() {
    union Data d;
    d.i = 42;
    return d;
}

int main() {
    union Data result = make();
    return result.i;  // Should return 42
}
