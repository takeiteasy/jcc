// Test FFI type checking - wrong argument count
// Expected: Compilation error (argument count mismatch)
// Usage: ./jcc --ffi-type-checking test_ffi_type_arg_count.c

int strcmp(const char *s1, const char *s2);

int main() {
    // This should fail: strcmp expects 2 arguments, but we're passing 1
    int result = strcmp("hello");
    return result;
}
