// Simple attribute test

int __attribute__((unused)) x = 5;

int main(void) {
    if (x != 5) return 1;  // Assert x == 5
    return 42;
}
