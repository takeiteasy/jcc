// Test for multi-register opcodes
// This test validates the new ADD3, SUB3, etc. opcodes work correctly

int main() {
    // Simple arithmetic - currently uses old opcodes
    int a = 10;
    int b = 20;
    int c = a + b;  // Should be 30
    
    if (c != 30) return 1;
    
    int d = c - 5;  // Should be 25
    if (d != 25) return 2;
    
    int e = d * 2;  // Should be 50
    if (e != 50) return 3;
    
    int f = e / 5;  // Should be 10
    if (f != 10) return 4;
    
    int g = f % 3;  // Should be 1
    if (g != 1) return 5;
    
    // Comparisons
    if (!(a < b)) return 6;  // 10 < 20 is true
    if (!(b > a)) return 7;  // 20 > 10 is true
    if (!(c == 30)) return 8;
    if (!(c != 31)) return 9;
    if (!(a <= 10)) return 10;
    if (!(b >= 20)) return 11;
    
    // Bitwise
    int h = 0xFF & 0x0F;  // Should be 0x0F (15)
    if (h != 15) return 12;
    
    int i = 0xF0 | 0x0F;  // Should be 0xFF (255)
    if (i != 255) return 13;
    
    int j = 0xFF ^ 0xF0;  // Should be 0x0F (15)
    if (j != 15) return 14;
    
    int k = 1 << 4;  // Should be 16
    if (k != 16) return 15;
    
    int l = 32 >> 2;  // Should be 8
    if (l != 8) return 16;
    
    return 0;  // All tests passed
}
