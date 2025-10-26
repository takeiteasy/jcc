// Test using just first parameter in expression
int test(int a, int b) {
    int x = a + 10;
    return x;
}

int main() {
    return test(32, 99);  // Should return 42
}
