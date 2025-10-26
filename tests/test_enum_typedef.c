// Test enum scoping and typedef
enum Color { RED, GREEN, BLUE };

typedef enum { SMALL = 1, MEDIUM = 2, LARGE = 3 } Size;

int get_color_value(enum Color c) {
    return c;
}

int get_size_value(Size s) {
    return s;
}

int main() {
    enum Color c = GREEN;
    Size s = MEDIUM;
    
    int result = get_color_value(c) + get_size_value(s);  // 1 + 2 = 3
    
    // Test reassignment
    c = BLUE;
    s = LARGE;
    
    result = get_color_value(c) * get_size_value(s);  // 2 * 3 = 6
    
    return result * 7;  // 6 * 7 = 42
}
