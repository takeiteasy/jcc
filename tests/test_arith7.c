// Test addition and equality comparison
int main() { 
    int a = 10; 
    int b = 20;
    int c = a + b;
    int eq = c == 30;  // Should be 1
    if (eq != 1) return 1;
    return 42;
}
