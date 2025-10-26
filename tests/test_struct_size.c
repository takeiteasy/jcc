// Test sizeof struct
struct Point { int x; int y; };

int main() {
    // In VM, int is 4 bytes, so struct Point should be 8 bytes
    int size = sizeof(struct Point);
    if (size != 8) return 1;
    return 42;  // Success!
}
