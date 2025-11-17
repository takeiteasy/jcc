// TLS operations test - read, write, address-of, arithmetic
_Thread_local int x = 10;
_Thread_local long y = 20;

int main() {
    // Test read
    if (x != 10) return 1;

    // Test write
    x = 30;
    if (x != 30) return 2;

    // Test arithmetic
    x += 5;
    if (x != 35) return 3;

    // Test address-of
    int *ptr = &x;
    *ptr = 50;
    if (x != 50) return 4;

    // Test multiple TLS variables
    if (y != 20) return 5;
    y = 100;
    if (y != 100) return 6;

    return 0;  // Success
}
