int main() { 
    int a = 10; 
    int b = 20;
    int c = a + b;
    // return c; // This would work
    if (c == 30) return 42;  // Using == instead of !=
    return 1;
}
