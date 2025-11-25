int main() {
    int result = 0;
#ifdef TEST_FLAG
    result = 100;
#else
    result = 200;
#endif
    // Test that #ifdef/#else works correctly
    // Expected: 200 (TEST_FLAG not defined)
    if (result != 200) return 1;
    return 42;
}
