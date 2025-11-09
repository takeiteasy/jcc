// Test FFI allow/deny list enforcement
//
// Test 1: Run with --ffi-allow=printf (should work)
//   ./jcc --ffi-allow=printf test_ffi_allow_deny.c
//   Expected: Program runs, prints "Hello, FFI!\n", returns 0
//
// Test 2: Run with --ffi-deny=printf (should be blocked)
//   ./jcc --ffi-deny=printf test_ffi_allow_deny.c
//   Expected: FFI SAFETY ERROR, warning printed, returns 0 (warning mode)
//
// Test 3: Run with --ffi-deny=printf --ffi-errors-fatal (should abort)
//   ./jcc --ffi-deny=printf --ffi-errors-fatal test_ffi_allow_deny.c
//   Expected: FFI SAFETY ERROR, program aborts with exit code 1
//
// Test 4: Run with --ffi-allow=malloc (printf not in allow list)
//   ./jcc --ffi-allow=malloc test_ffi_allow_deny.c
//   Expected: FFI SAFETY ERROR (not in allow list), warning printed, returns 0

int printf0(const char *fmt);

int main() {
    printf0("Hello, FFI!\n");
    return 0;
}
