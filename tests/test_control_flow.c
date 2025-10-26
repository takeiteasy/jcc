int main() {
    int result = 0;
    
    // Test if statement
    if (1) {
        result = result + 10;  // Should execute
    }
    
    if (0) {
        result = result + 100;  // Should NOT execute
    }
    
    // Test if-else
    if (5 > 10) {
        result = result + 1000;  // Should NOT execute
    } else {
        result = result + 20;    // Should execute
    }
    
    // Test for loop - sum 1 to 5
    int i;
    for (i = 1; i <= 5; i = i + 1) {
        result = result + i;
    }
    // Adds: 1 + 2 + 3 + 4 + 5 = 15
    
    // Total: 10 + 20 + 15 = 45
    return result;
}
