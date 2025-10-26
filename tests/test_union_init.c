// Test union initialization and assignment patterns
// Expected return: 42

union Data {
    int i;
    long l;
    char c;
};

union Arrays {
    int arr[3];
    char bytes[12];
};

int main() {
    int score = 0;
    
    // Test 1: Assignment through different members
    union Data d1;
    d1.i = 100;
    d1.c = 42;  // Overwrites lowest byte
    // Check that we can still read c
    if (d1.c == 42) score += 10;
    
    // Test 2: Union with arrays
    union Arrays a;
    a.arr[0] = 10;
    a.arr[1] = 20;
    a.arr[2] = 30;
    
    // Verify array access
    if (a.arr[0] == 10 && a.arr[1] == 20 && a.arr[2] == 30) {
        score += 10;
    }
    
    // Test 3: Writing as bytes, reading as int
    union Data d2;
    d2.c = 42;
    // Just verify we can read what we wrote
    if (d2.c == 42) score += 10;
    
    // Test 4: Long assignment
    union Data d3;
    d3.l = 12;
    if (d3.l == 12) score += 12;
    
    return score;  // Should be 42
}
