// Test 2 only: Direct member access on compound literal
// Expected return: 42

struct Point {
    int x;
    int y;
};

int main() {
    // Test 2: Direct member access on compound literal
    int val = ((struct Point){40, 2}).x + ((struct Point){40, 2}).y;
    if (val != 42) return 2;
    
    return 42;
}
