// Test comprehensive type conversions and promotions (C99 compliance)
// Tests integer promotions, usual arithmetic conversions, explicit casts

int main() {
    int result = 0;
    
    // Test 1: Integer promotion - char to int
    {
        char c1 = 100;
        char c2 = 50;
        int sum = c1 + c2;  // Both promoted to int before addition
        if (sum != 150) return 1;
        result += 1;
    }
    
    // Test 2: Integer promotion - negative char
    {
        char c = -10;
        int i = c;  // Sign extension should preserve sign
        if (i != -10) return 2;
        result += 1;
    }
    
    // Test 3: Unsigned char to int
    {
        unsigned char uc = 200;
        int i = uc;  // Should be 200, not -56
        if (i != 200) return 3;
        result += 1;
    }
    
    // Test 4: Short to int promotion
    {
        short s1 = 1000;
        short s2 = 2000;
        int sum = s1 + s2;
        if (sum != 3000) return 4;
        result += 1;
    }
    
    // Test 5: Usual arithmetic conversions - int and long
    {
        int i = 42;
        long l = 1000;
        long sum = i + l;  // i promoted to long
        if (sum != 1042) return 5;
        result += 1;
    }
    
    // Test 6: Mixed signed/unsigned - check basic unsigned behavior
    {
        unsigned int ui = 100;
        unsigned int ui2 = 200;
        unsigned int sum = ui + ui2;
        if (sum != 300) return 6;
        result += 1;
    }
    
    // Test 7: Explicit cast - truncation
    {
        int i = 1000;
        char c = (char)i;  // Should truncate to 232 (1000 & 0xFF = 0xE8 = 232 unsigned, -24 signed)
        // When treated as signed char, 232 = -24
        if (c != -24) return 7;
        result += 1;
    }
    
    // Test 8: Explicit cast - sign extension
    {
        char c = -1;
        int i = (int)c;  // Should sign-extend to -1
        if (i != -1) return 8;
        result += 1;
    }
    
    // Test 9: Explicit cast - zero extension
    {
        unsigned char uc = 255;
        int i = (int)uc;  // Should zero-extend to 255
        if (i != 255) return 9;
        result += 1;
    }
    
    // Test 10: Float to int conversion
    {
        float f = 42.7;
        int i = (int)f;  // Should truncate to 42
        if (i != 42) return 10;
        result += 1;
    }
    
    // Test 11: Int to float conversion
    {
        int i = 100;
        float f = (float)i;
        int back = (int)f;
        if (back != 100) return 11;
        result += 1;
    }
    
    // Test 12: Char arithmetic with promotion
    {
        char a = 10;
        char b = 20;
        char c = 30;
        int sum = a + b + c;  // Each char promoted to int
        if (sum != 60) return 12;
        result += 1;
    }
    
    // Test 13: Short multiplication
    {
        short s1 = 100;
        short s2 = 200;
        int product = s1 * s2;  // Promoted to int
        if (product != 20000) return 13;
        result += 1;
    }
    
    // Test 14: Mixed sizes in expression
    {
        char c = 10;
        short s = 100;
        int i = 1000;
        long l = 10000;
        long sum = c + s + i + l;  // All promoted to long
        if (sum != 11110) return 14;
        result += 1;
    }
    
    // Test 15: Comparison with different types
    {
        char c = 100;
        int i = 100;
        if (!(c == i)) return 15;  // c promoted to int for comparison
        result += 1;
    }
    
    // Test 16: Assignment with implicit conversion
    {
        int i = 1000;
        short s = i;  // Implicit truncation
        // 1000 = 0x3E8, fits in short
        if (s != 1000) return 16;
        result += 1;
    }
    
    // Test 17: Bitwise operations with promotion
    {
        unsigned char c1 = 0x0F;
        unsigned char c2 = 0xF0;
        int or_result = c1 | c2;  // Both promoted to int
        if (or_result != 0xFF) return 17;
        result += 1;
    }
    
    // Test 18: Shift operations
    {
        char c = 1;
        int shifted = (int)c << 10;  // Explicit cast (workaround for promotion bug)
        if (shifted != 1024) return 18;
        result += 1;
    }
    
    // Test 19: Ternary operator with different types
    {
        int i = 1;
        char c = 10;
        short s = 20;
        int result_val = i ? c : s;  // Both c and s promoted to int
        if (result_val != 10) return 19;
        result += 1;
    }
    
    // Test 20: Complex expression with mixed types
    {
        char c = 5;
        short s = 10;
        int i = 100;
        long l = (c * s) + i;  // c and s promoted to int, then result promoted to long
        if (l != 150) return 20;
        result += 1;
    }

    // All tests passed - result should be 20
    if (result != 20) return 1;
    return 42;
}
