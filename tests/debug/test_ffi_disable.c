// Test --disable-ffi flag
//
// Test 1: Run normally (should work)
//   ./jcc test_ffi_disable.c
//   Expected: Prints "Testing FFI...\n", returns 42
//
// Test 2: Run with --disable-ffi (should block all FFI)
//   ./jcc --disable-ffi test_ffi_disable.c
//   Expected: FFI SAFETY ERROR for printf0, warning printed, returns 0 (warning mode)
//
// Test 3: Run with --disable-ffi --ffi-errors-fatal (should abort)
//   ./jcc --disable-ffi --ffi-errors-fatal test_ffi_disable.c
//   Expected: FFI SAFETY ERROR, program aborts with exit code 1

int printf0(const char *fmt);

int main() {
    printf0("Testing FFI...\n");
    return 42;
}
