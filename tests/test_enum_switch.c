// Test enum with mixed auto/explicit values
enum Mixed {
    A,          // 0
    B,          // 1
    C = 10,     // 10
    D,          // 11
    E = 20,     // 20
    F           // 21
};

int main() {
    int sum = A + B + C + D + E + F;  // 0 + 1 + 10 + 11 + 20 + 21 = 63
    
    // Test using enum in switch
    enum Mixed x = C;
    switch (x) {
        case A:
            return 1;
        case B:
            return 2;
        case C:
            return 42;  // This should execute
        case D:
            return 4;
        default:
            return 0;
    }
}
