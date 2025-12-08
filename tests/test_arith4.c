int main() { 
    int a = 10; 
    int b = 20;
    int c = a + b;
    int d = c;  // Read c once
    return d;   // Should be 30
}
