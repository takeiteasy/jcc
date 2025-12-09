// Test addition and comparison
int main() { 
    int a = 10; 
    int b = 20;
    int c = a + b;
    int result = c != 30;  // Should be 0 (false)
    if (result != 0) return 1;
    return 42;
}
