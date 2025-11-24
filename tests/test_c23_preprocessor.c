// Test C23 features: empty initializers
// Note: #elifdef, #elifndef, #warning are implemented but need more testing

int main() {
    // Test basic functionality
    int test = 1;

    // Test empty initializer (C23 feature)
    int empty_array[5] = {};
    // All elements should be zero
    for (int i = 0; i < 5; i++) {
        if (empty_array[i] != 0) return 7 + i;
    }

    // Test empty struct initializer
    struct {
        int a;
        int b;
        int c;
    } empty_struct = {};
    if (empty_struct.a != 0 || empty_struct.b != 0 || empty_struct.c != 0)
        return 12;

    return 42;  // Success
}
