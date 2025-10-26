// Test if struct member offset issue exists
// Expected: 42 if correct, other value if bug

struct Test {
    int a;  // offset 0 (should be 0-7 in VM)
    int b;  // offset 4 in chibicc, but should be 8-15 in VM
};

int main() {
    struct Test t;
    t.a = 100;
    t.b = 200;
    
    // If offsets overlap, b will overwrite part of a
    // Then a will have a corrupted value
    
    int a_val = t.a;
    int b_val = t.b;
    
    // Return a to see if it's still 100
    return a_val;  // Should be 100
}
