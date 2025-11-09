// Test for CFI - normal function calls and returns
// This should execute successfully with -C flag

int helper() {
    return 42;
}

int main() {
    int x = helper();
    return x;
}
