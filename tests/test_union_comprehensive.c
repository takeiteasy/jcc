// Comprehensive union test
// Expected return: 42

union Value {
    int i;
    char c;
    long l;
};

union Mixed {
    int x;
    struct {
        char a;
        char b;
    } pair;
};

int main() {
    // Test 1: Basic union operations
    union Value v1;
    v1.i = 100;
    int r1 = v1.i;  // Should be 100
    
    // Test 2: Union overwrites
    union Value v2;
    v2.i = 500;
    v2.c = 10;  // Overwrites lower byte
    // v2.i now has modified value
    
    // Test 3: Union with struct member
    union Mixed m;
    m.x = 0x4241;  // 'BA' in little-endian
    char first = m.pair.a;   // Should be 0x41 ('A')
    char second = m.pair.b;  // Should be 0x42 ('B')
    
    // Test 4: Assignment through different members
    union Value v3;
    v3.l = 42;
    int result = v3.i;  // Read as int (should be 42)
    
    // Calculate result
    int sum = 0;
    if (r1 == 100) sum += 10;
    if (first == 0x41) sum += 10;
    if (second == 0x42) sum += 10;
    if (result == 42) sum += 12;
    
    return sum;  // Should return 42
}
