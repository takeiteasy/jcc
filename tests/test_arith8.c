// Test addition, copy, and equality comparison  
int main() { 
    int a = 10; 
    int b = 20;
    int c = a + b;
    int d = c;  // Read c into d
    int eq = d == 30;  // Compare d with 30
    if (eq != 1) return 1;
    return 42;
}
