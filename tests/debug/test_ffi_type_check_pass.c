// Test FFI type checking with valid calls
// Expected: Program runs successfully, returns 42
// Usage: ./jcc --ffi-type-checking test_ffi_type_check_pass.c

int printf0(const char *fmt);
void *malloc(unsigned long size);
void free(void *ptr);
int strcmp(const char *s1, const char *s2);
unsigned long strlen(const char *s);

int main() {
    // Test 1: printf with correct type
    printf0("Hello, FFI type checking!\n");

    // Test 2: malloc with correct unsigned long
    unsigned long size = 100;
    void *ptr = malloc(size);
    if (!ptr) return 1;

    // Test 3: strcmp with two string pointers
    const char *str1 = "hello";
    const char *str2 = "world";
    int result = strcmp(str1, str2);

    // Test 4: strlen with string pointer
    unsigned long len = strlen("test");
    if (len != 4) return 2;

    // Test 5: free with void pointer
    free(ptr);

    printf0("All type checks passed!\n");
    return 42;
}
