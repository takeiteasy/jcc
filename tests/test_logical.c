int main() {
    int a = 5;
    int b = 0;
    int c = 10;
    
    // Logical AND (&&) - short circuits on first false
    int and1 = (a && c);    // 1 (both true)
    int and2 = (b && c);    // 0 (first is false)
    int and3 = (a && b);    // 0 (second is false)
    
    // Logical OR (||) - short circuits on first true
    int or1 = (a || b);     // 1 (first is true)
    int or2 = (b || c);     // 1 (second is true)
    int or3 = (b || 0);     // 0 (both false)
    
    // Sum: 1 + 0 + 0 + 1 + 1 + 0 = 3
    int result = and1 + and2 + and3 + or1 + or2 + or3;

    if (result != 3) return 1;  // Assert result == 3
    return 42;
}
