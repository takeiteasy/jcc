/*
 * Test: __builtin_frame_address(0)
 * Tests that the builtin returns a non-null frame pointer
 */

int main() {
    void *fp = __builtin_frame_address(0);
    // Frame pointer should be non-null
    if (fp == 0) return 1;
    return 42;
}
