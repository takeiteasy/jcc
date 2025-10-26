// Comprehensive test for compound assignment operators
// Tests: Edge cases, arrays, pointers, struct members, globals, and complex expressions
// Returns: 42

int global = 100;

struct Point {
    int x;
    int y;
};

int main() {
    // Test 1: Compound assignment with arrays
    int arr[5];
    arr[0] = 10;
    arr[0] += 32;  // arr[0] = 42
    if (arr[0] != 42) return 1;
    
    // Test 2: Compound assignment with array indexing via variable
    int idx = 1;
    arr[idx] = 20;
    arr[idx] *= 2;  // arr[1] = 40
    arr[idx] += 2;  // arr[1] = 42
    if (arr[idx] != 42) return 2;
    
    // Test 3: Compound assignment with pointer dereference
    int val = 30;
    int *ptr = &val;
    *ptr += 12;  // *ptr = 42
    if (val != 42) return 3;
    if (*ptr != 42) return 4;
    
    // Test 4: Compound assignment with struct members
    struct Point p;
    p.x = 40;
    p.x += 2;  // p.x = 42
    if (p.x != 42) return 5;
    
    // Test 5: Compound assignment with global variable
    global -= 58;  // global = 100 - 58 = 42
    if (global != 42) return 6;
    
    // Test 6: Compound assignment in loop
    int sum = 0;
    int i;
    for (i = 0; i < 10; i = i + 1) {
        sum += i;  // sum = 0+1+2+3+4+5+6+7+8+9 = 45
    }
    if (sum != 45) return 7;
    
    // Test 7: Multiple compound assignments in expression
    int a = 10;
    int b = 20;
    a += 5;  // a = 15
    b += 7;  // b = 27
    if (a + b != 42) return 8;
    
    // Test 8: Compound assignment with complex right-hand side
    int x = 10;
    int y = 5;
    int z = 3;
    x += y * z + 7;  // x = 10 + (5*3 + 7) = 10 + 22 = 32
    if (x != 32) return 9;
    
    // Test 9: All bitwise compound assignments
    int bits = 0;
    bits |= 2;   // bits = 2
    bits |= 8;   // bits = 10
    bits |= 32;  // bits = 42
    if (bits != 42) return 10;
    
    // Test 10: Shift operators with variables
    int shift = 21;
    int shift_amt = 1;
    shift <<= shift_amt;  // shift = 42
    if (shift != 42) return 11;
    
    // Test 11: Modulo with compound assignment
    int mod = 242;
    mod %= 200;  // mod = 42
    if (mod != 42) return 12;
    
    // Test 12: Division with compound assignment
    int div = 126;
    div /= 3;  // div = 42
    if (div != 42) return 13;
    
    // Test 13: Nested struct member compound assignment
    struct Point points[2];
    points[0].x = 20;
    points[0].y = 22;
    points[0].x += points[0].y;  // points[0].x = 42
    if (points[0].x != 42) return 14;
    
    // Test 14: Compound assignment as condition
    int cond = 41;
    if ((cond += 1) != 42) return 15;
    
    // Test 15: XOR compound assignment
    int xor_val = 60;  // 0b111100
    xor_val ^= 22;     // 0b111100 ^ 0b010110 = 0b101010 = 42
    if (xor_val != 42) return 16;
    
    // Test 16: Chain multiple operations
    int chain = 5;
    chain += 5;   // 10
    chain *= 2;   // 20
    chain += 10;  // 30
    chain += 12;  // 42
    if (chain != 42) return 17;
    
    // Test 17: Compound assignment with subtraction (instead of negative)
    int neg = 50;
    neg -= 8;  // neg = 42
    if (neg != 42) return 18;
    
    // Test 18: Right shift with compound assignment
    int rshift = 168;
    rshift >>= 2;  // 168 >> 2 = 42
    if (rshift != 42) return 19;
    
    // All tests passed!
    return 42;
}
