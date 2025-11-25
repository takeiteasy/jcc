// Check actual int size

int main() {
    int x;
    // Return sizeof(int) to see what it actually is
    int size = sizeof(x);
    if (size != 4) return 1;  // Assert sizeof(int) == 4
    return 42;
}
