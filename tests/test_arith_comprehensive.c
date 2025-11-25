int main() {
    // Basic arithmetic
    int a = 5 + 3;      // 8
    int b = 10 - 4;     // 6
    int c = 7 * 2;      // 14
    int d = 20 / 4;     // 5
    int e = 17 % 5;     // 2
    
    // Combined operations
    int result = a + b * c - d / e;  // 8 + 6*14 - 5/2 = 8 + 84 - 2 = 90

    if (result != 90) return 1;  // Assert result == 90
    return 42;
}
