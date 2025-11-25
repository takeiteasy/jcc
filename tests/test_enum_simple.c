// Test basic enum functionality
enum Color { RED, GREEN, BLUE };

int main() {
    enum Color c = RED;
    if (c == 0) {
        c = GREEN;
    }
    if (c == 1) {
        c = BLUE;
    }
    if (c != 2) return 1;  // Assert c == 2 (BLUE)
    return 42;
}
