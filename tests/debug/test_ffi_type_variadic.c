// Test FFI type checking with variadic functions
// Expected: Compilation error if too few fixed args, success otherwise
// Usage: ./jcc --ffi-type-checking test_ffi_type_variadic.c

// printf has 1 fixed arg (format string), rest are variadic
int printf(const char *fmt, ...);

int main() {
    // These should all pass - variadic args are not type-checked
    printf("No args\n");
    printf("One arg: %d\n", 42);
    printf("Two args: %d %s\n", 42, "hello");
    printf("Three args: %d %s %f\n", 42, "hello", 3.14);

    return 0;
}
