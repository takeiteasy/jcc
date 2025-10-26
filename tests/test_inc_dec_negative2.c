// Test simple ++ with negative
int main() {
    int a = -5;
    int b = a;  // b = -5
    a++;        // a should be -4
    
    // Return difference
    return a - b;  // Should return 1 if increment worked
}
