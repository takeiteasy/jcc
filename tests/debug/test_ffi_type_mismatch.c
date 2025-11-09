// Test FFI type checking - type mismatch
// Expected: Compilation error (type mismatch)
// Usage: ./jcc --ffi-type-checking test_ffi_type_mismatch.c

unsigned long strlen(const char *s);

int main() {
    // This should fail: strlen expects char*, but we're passing int
    int not_a_string = 42;
    unsigned long len = strlen(not_a_string);
    return len;
}
