// Test enum edge cases and advanced features
enum Large {
    A = 100,
    B,        // 101
    C = 10,
    D         // 11
};

int main() {
    // Test sequential values
    enum Large x = A;
    if (x != 100) return 1;
    
    x = B;
    if (x != 101) return 2;
    
    x = C;
    if (x != 10) return 3;
    
    x = D;
    if (x != 11) return 4;
    
    // Test arithmetic with enums
    int sum = A + C;  // 100 + 10 = 110
    if (sum != 110) return 5;
    
    // Test using enum in calculations
    int result = (B - A) + (D - C);  // (101-100) + (11-10) = 1 + 1 = 2
    if (result != 2) return 6;
    
    // Calculate final result
    result = C + D + x + result;  // 10 + 11 + 11 + 2 = 34
    if (result != 34) return 7;
    
    return 42;  // All tests passed
}
