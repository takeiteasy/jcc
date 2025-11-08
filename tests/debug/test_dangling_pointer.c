// test_dangling_pointer.c
// Tests dangling stack pointer detection
// Expected behavior: Should detect when dereferencing a pointer to a
// stack variable after the function has returned

int *get_local_address() {
    int x = 42;
    return &x;  // Return address of local variable (dangling pointer!)
}

int main() {
    int *ptr = get_local_address();
    int value = *ptr;  // Dereference dangling pointer - should be caught!
    return value;
}
