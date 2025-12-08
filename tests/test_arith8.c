int main() { 
    int a = 10; 
    int b = 20;
    int c = a + b;
    int d = c;  // Read c into d
    int eq = d == 30;  // Compare d with 30
    return eq;  // Should be 1
}
