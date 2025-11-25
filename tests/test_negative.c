// Test negative numbers
int main() {
    int a = -5;
    int result = 0 - a;  // Should be 5
    if (result != 5) return 1;  // Assert result == 5
    return 42;
}
