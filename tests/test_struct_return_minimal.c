// Test struct return - absolute minimal for comparison
struct Data { int i; };

struct Data make() {
    struct Data d;
    d.i = 42;
    return d;
}

int main() {
    struct Data result = make();
    return result.i;  // Should return 42
}
