// Test FFI type checking - pointer compatibility
// Expected: Success (void* compatible with any pointer)
// Usage: ./jcc --ffi-type-checking test_ffi_type_pointer_compat.c

void *malloc(unsigned long size);
void free(void *ptr);
void *memset(void *s, int c, unsigned long n);

int main() {
    // Test 1: malloc returns void*, assign to int*
    int *int_ptr = (int *)malloc(40);
    if (!int_ptr) return 1;

    // Test 2: memset accepts void*, pass int*
    // void* should be compatible with any pointer type
    memset(int_ptr, 0, 40);

    // Test 3: free accepts void*, pass int*
    free(int_ptr);

    return 0;
}
