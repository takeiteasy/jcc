// Test each section individually
int main() {
    // Test 1-4: Basic increment/decrement
    int a = 5;
    int b = a++;
    if (b != 5) return 1;
    if (a != 6) return 2;
    
    int c = 5;
    int d = ++c;
    if (d != 6) return 3;
    if (c != 6) return 4;
    
    int e = 10;
    int f = e--;
    if (f != 10) return 5;
    if (e != 9) return 6;
    
    int g = 10;
    int h = --g;
    if (h != 9) return 7;
    if (g != 9) return 8;
    
    // Test 5-6: Multiple operations
    int i = 0;
    i++;
    i++;
    i++;
    if (i != 3) return 9;
    
    int j = 5;
    j--;
    j--;
    if (j != 3) return 10;
    
    // Test 7-8: In expressions
    int k = 5;
    int result1 = ++k + 10;
    if (k != 6) return 11;
    if (result1 != 16) return 12;
    
    int m = 5;
    int result2 = m++ + 10;
    if (m != 6) return 13;
    if (result2 != 15) return 14;
    
    return 42;  // All passed
}
