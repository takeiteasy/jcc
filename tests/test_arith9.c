// Test addition and variable copy
int main() { 
    int a = 10; 
    int b = 20;
    int c = a + b;
    int d = c;
    if (d != 30) return 1;
    return 42;
}
