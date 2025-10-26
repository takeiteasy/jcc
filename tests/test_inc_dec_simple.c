// Simple test for post-increment
int main() {
    int a = 5;
    int b = a++;  // b should be 5, a should be 6
    
    // Check if b is 5
    if (b != 5) {
        return 1;
    }
    
    // Check if a is 6
    if (a != 6) {
        return 2;
    }
    
    return 42;  // Success
}
