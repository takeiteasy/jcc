// Test local variables in function with params

int test(int a) {
    int x = 10;
    return x;
}

int main() {
    return test(99);  // Should return 10, ignoring the param
}
