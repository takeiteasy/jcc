// Simple t    if (a != 11) {
        return 2;
    }
    
    return 42;  // Success
}r pre-increment
int main() {
    int c = 5;
    int d = ++c;  // Both c and d should be 6
    
    // Check if d is 6
    if (d != 6) {
        return 1;
    }
    
    // Check if c is 6
    if (c != 6) {
        return 2;
    }
    
    return 0;  // Success
}
