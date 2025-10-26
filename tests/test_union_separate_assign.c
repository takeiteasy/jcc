// Test union return with separate declaration and assignment
union Data { int i; };

union Data make() {
    union Data d;
    d.i = 42;
    return d;
}

int main() {
    union Data result;
    result = make();  // Separate assignment, not initialization
    return result.i;
}
