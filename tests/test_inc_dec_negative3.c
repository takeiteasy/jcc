// Test negative value directly  
int main() {
    int a = -5;
    a++;
    // If a is -4, then -a is 4
    // Return 4 to check
    int result = 0 - a;
    return result;  // Should return 4
}
