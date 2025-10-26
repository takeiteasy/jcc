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
    return c;  // Should return 2 (BLUE)
}
