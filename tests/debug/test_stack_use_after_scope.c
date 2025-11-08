// Test dangling pointer detection
// This test should FAIL with error when run with -i --stack-errors --dangling-pointers

int main() {
    int *ptr;

    {
        int x = 42;
        ptr = &x;  // Take address of stack variable
    }
    // x is now out of scope, ptr is dangling
    // The SCOPEOUT instruction should detect this

    return 0;  // Should fail at scope exit, not here
}
