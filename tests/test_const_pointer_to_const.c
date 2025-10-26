// Test pointer to const (can change pointer, not pointee)

int main() {
    int x = 10;
    int y = 20;
    
    const int *p = &x;  // Pointer to const int
    p = &y;             // OK: can change pointer
    // *p = 30;         // ERROR: cannot modify through pointer to const
    
    return *p;  // Should return 20
}
