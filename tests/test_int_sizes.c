// Test that different integer types have correct sizes
// char=1, short=2, int=4, long=8

int main() {
    // Test sizeof for each type
    if (sizeof(char) != 1) return 1;
    if (sizeof(short) != 2) return 2;
    if (sizeof(int) != 4) return 3;
    if (sizeof(long) != 8) return 4;
    
    // Test that values work correctly
    char c = 100;
    short s = 1000;
    int i = 100000;
    long l = 1000000000;
    
    if (c != 100) return 5;
    if (s != 1000) return 6;
    if (i != 100000) return 7;
    if (l != 1000000000) return 8;
    
    // Test arrays
    int arr[3];
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    
    if (sizeof(arr) != 12) return 9;  // 3 * 4 bytes
    if (arr[0] + arr[1] + arr[2] != 60) return 10;
    
    // Test struct with mixed types
    struct Mixed {
        char c;
        short s;
        int i;
        long l;
    };
    
    struct Mixed m;
    m.c = 1;
    m.s = 2;
    m.i = 3;
    m.l = 4;
    
    if (m.c + m.s + m.i + m.l != 10) return 11;
    
    // Test short arithmetic
    short a = 100;
    short b = 200;
    if (a + b != 300) return 12;
    
    return 42;  // Success!
}
