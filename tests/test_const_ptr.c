// Test const pointer (cannot change pointer, can change pointee)

int main() {
    int x = 10;
    int y = 20;
    
    int *const p = &x;  // Const pointer to int
    *p = 30;            // OK: can modify through pointer
    // p = &y;          // ERROR: cannot change const pointer
    
    return *p;  // Should return 30
}
