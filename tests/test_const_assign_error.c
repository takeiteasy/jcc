// Test that assignment to const variable is rejected

int main() {
    const int x = 42;
    x = 10;  // ERROR: should fail to compile!
    return 0;
}
