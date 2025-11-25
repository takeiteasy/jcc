// Test part 2
int main() {
    // Test decrement in expressions
    int n = 10;
    int result3 = --n * 2;
    if (n != 9) return 1;
    if (result3 != 18) return 2;
    
    int p = 10;
    int result4 = p-- * 2;
    if (p != 9) return 3;
    if (result4 != 20) return 4;
    
    // Test in for loops
    int count = 0;
    for (int x = 0; x < 5; x++) {
        count++;
    }
    if (count != 5) return 5;
    
    int count2 = 0;
    for (int y = 10; y > 5; y--) {
        count2++;
    }
    if (count2 != 5) return 6;

    return 42;  // All passed
}
