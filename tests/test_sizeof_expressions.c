// Test sizeof with various expressions and contexts
int main() {
    int x = 10;
    int arr[10];
    
    // sizeof with variables
    if (sizeof(x) != 8) return 1;
    
    // sizeof with expressions (should not evaluate expression)
    int counter = 0;
    int size = sizeof(counter++);  // counter should NOT be incremented
    if (counter != 0) return 2;    // Verify counter wasn't changed
    if (size != 8) return 3;
    
    // sizeof with array variables
    if (sizeof(arr) != 80) return 4;  // 10 * 8
    
    // sizeof with array elements
    if (sizeof(arr[0]) != 8) return 5;
    
    // sizeof with pointer arithmetic expressions
    if (sizeof(arr[5]) != 8) return 6;
    
    // sizeof with complex expressions
    if (sizeof(x + 10) != 8) return 7;
    if (sizeof(x * 2) != 8) return 8;
    
    // sizeof with dereferenced pointers
    int *ptr = &x;
    if (sizeof(*ptr) != 8) return 9;
    
    // sizeof with struct
    struct Point { int x; int y; };
    if (sizeof(struct Point) != 16) return 10;
    
    struct Point p;
    if (sizeof(p) != 16) return 11;
    if (sizeof(p.x) != 8) return 12;
    
    return 42;  // All tests passed
}
