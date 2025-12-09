// Test local variable offset handling
int main() { 
    int a = 10; 
    int b = 20;
    int c = a + b;
    // c is at offset -3. When we read c:
    int eq = c == 30;
    if (eq != 1) return 1;
    return 42;
}
