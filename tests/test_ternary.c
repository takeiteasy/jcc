// Test ternary operator (?:)
int main() {
    // Basic ternary
    int a = 1 ? 10 : 20;  // Should be 10
    int b = 0 ? 10 : 20;  // Should be 20
    
    // Nested ternary
    int c = 1 ? (0 ? 1 : 2) : 3;  // Should be 2
    
    // Ternary with expressions
    int x = 5;
    int y = 10;
    int max = (x > y) ? x : y;  // Should be 10
    
    // Ternary in arithmetic
    int result = 2 + (x < y ? 5 : 3);  // Should be 7
    
    // Chained ternary (right-associative)
    int grade = 85;
    int category = grade >= 90 ? 4 : 
                   grade >= 80 ? 3 : 
                   grade >= 70 ? 2 : 1;  // Should be 3
    
    // Verify results
    if (a != 10) return 1;
    if (b != 20) return 2;
    if (c != 2) return 3;
    if (max != 10) return 4;
    if (result != 7) return 5;
    if (category != 3) return 6;
    
    return 42;  // Success
}
