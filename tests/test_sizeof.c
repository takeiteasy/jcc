int main() {
    // Basic types
    if (sizeof(char) != 1) return 1;
    if (sizeof(int) != 4) return 2;  // int is 4 bytes
    if (sizeof(long) != 8) return 3;
    
    // Pointers
    if (sizeof(int*) != 8) return 4;
    if (sizeof(char*) != 8) return 5;
    
    // Arrays
    int arr[5];
    if (sizeof(arr) != 20) return 6;  // 5 * 4
    
    // Struct
    struct Point { int x; int y; };
    if (sizeof(struct Point) != 8) return 7;  // 2 * 4
    
    // Variables
    int x;
    if (sizeof(x) != 4) return 8;
    
    char c;
    if (sizeof(c) != 1) return 9;
    
    // Expressions
    if (sizeof(x + 1) != 4) return 10;
    if (sizeof(arr[0]) != 4) return 11;
    
    return 42;  // All tests passed!
}