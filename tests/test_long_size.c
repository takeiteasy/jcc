// Check sizeof(long)

int main() {
    long x;
    int size = sizeof(x);
    if (size != 8) return 1;  // Assert sizeof(long) == 8
    return 42;
}
