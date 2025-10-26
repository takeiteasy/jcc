// Test compound assignment operators
// Tests: +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=
// All compound assignment operators should be fully functional
// Returns: 42

int main() {
    // Test += (addition assignment)
    int x = 10;
    x += 32;  // x = 10 + 32 = 42
    if (x != 42) return 1;
    
    // Test -= (subtraction assignment)
    int y = 50;
    y -= 8;  // y = 50 - 8 = 42
    if (y != 42) return 2;
    
    // Test *= (multiplication assignment)
    int z = 6;
    z *= 7;  // z = 6 * 7 = 42
    if (z != 42) return 3;
    
    // Test /= (division assignment)
    int w = 84;
    w /= 2;  // w = 84 / 2 = 42
    if (w != 42) return 4;
    
    // Test %= (modulo assignment)
    int m = 142;
    m %= 100;  // m = 142 % 100 = 42
    if (m != 42) return 5;
    
    // Test &= (bitwise AND assignment)
    int a = 63;    // 0b111111
    a &= 42;       // 0b111111 & 0b101010 = 0b101010 = 42
    if (a != 42) return 6;
    
    // Test |= (bitwise OR assignment)
    int b = 40;    // 0b101000
    b |= 2;        // 0b101000 | 0b000010 = 0b101010 = 42
    if (b != 42) return 7;
    
    // Test ^= (bitwise XOR assignment)
    int c = 50;    // 0b110010
    c ^= 24;       // 0b110010 ^ 0b011000 = 0b101010 = 42
    if (c != 42) return 8;
    
    // Test <<= (left shift assignment)
    int d = 21;
    d <<= 1;       // 21 << 1 = 42
    if (d != 42) return 9;
    
    // Test >>= (right shift assignment)
    int e = 84;
    e >>= 1;       // 84 >> 1 = 42
    if (e != 42) return 10;
    
    // Test chained compound assignments
    int f = 10;
    f += 5;  // 15
    f *= 2;  // 30
    f += 12; // 42
    if (f != 42) return 11;
    
    // Test compound assignment with expression on right side
    int g = 20;
    int h = 11;
    g += h * 2;  // g = 20 + (11 * 2) = 20 + 22 = 42
    if (g != 42) return 12;
    
    // Test compound assignment returns value
    int i = 30;
    int j = (i += 12);  // i = 42, j = 42
    if (j != 42) return 13;
    if (i != 42) return 14;
    
    // Test all operators work correctly
    return 42;
}
