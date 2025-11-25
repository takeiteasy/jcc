int main() {
    int a = 42;
    int *p;
    
    // Test address-of and dereference
    p = &a;        // Get address of a
    int b = *p;    // Dereference pointer
    
    // Modify through pointer
    *p = 100;

    // a should now be 100
    if (a != 100) return 1;  // Assert a == 100
    return 42;
}
