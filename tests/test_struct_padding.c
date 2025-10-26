// Test struct padding and alignment with different-sized types

struct mixed {
    char c;     // offset 0, size 1
                // 1 byte padding
    short s;    // offset 2, size 2
                // 0 bytes padding (already aligned)
    int i;      // offset 4, size 4
    long l;     // offset 8, size 8
};

struct nested_padding {
    char a;     // offset 0, size 1
                // 3 bytes padding
    int b;      // offset 4, size 4
    char c;     // offset 8, size 1
                // 7 bytes padding
    long d;     // offset 16, size 8
};

int main() {
    // Test struct mixed layout
    // With proper padding: char(1) + pad(1) + short(2) + int(4) + long(8) = 16
    if (sizeof(struct mixed) != 16) return 1;
    
    // Test nested_padding layout  
    // With proper padding: a(1) + pad(3) + b(4) + c(1) + pad(7) + d(8) = 24
    if (sizeof(struct nested_padding) != 24) return 2;
    
    // Test that we can write and read values correctly with mixed sizes
    struct mixed m;
    m.c = 10;
    m.s = 20;
    m.i = 30;
    m.l = 40;
    
    if (m.c != 10) return 3;
    if (m.s != 20) return 4;
    if (m.i != 30) return 5;
    if (m.l != 40) return 6;
    
    // Test nested struct
    struct nested_padding n;
    n.a = 100;
    n.b = 200;
    n.c = 50;
    n.d = 300;
    
    if (n.a != 100) return 7;
    if (n.b != 200) return 8;
    if (n.c != 50) return 9;
    if (n.d != 300) return 10;
    
    // Test struct with just different sizes to ensure no 8-byte assumption
    struct size_test {
        char a;
        char b;
        short c;
        int d;
    };
    
    // Should be: char(1) + char(1) + short(2) + int(4) = 8 (no padding needed after alignment)
    if (sizeof(struct size_test) != 8) return 11;
    
    return 42;  // All padding and alignment tests passed
}
