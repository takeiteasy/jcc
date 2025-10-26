// Test basic function with multiple arguments
int add_three(int a, int b, int c) {
    return a + b + c;
}

int main() {
    int result = add_three(10, 20, 30);
    if (result == 60) return 1;
    return 0;
}
