// Test pre-increment and pre-decrement operators
int main() {
    // Test pre-increment
    int a = 10;
    int b = ++a;  // Both a and b should be 11

    if (a != 11) {
        return 1;
    }

    if (b != 11) {
        return 2;
    }

    // Test pre-decrement
    int c = 5;
    int d = --c;  // Both c and d should be 4

    if (c != 4) {
        return 3;
    }

    if (d != 4) {
        return 4;
    }

    return 42;  // Success
}
