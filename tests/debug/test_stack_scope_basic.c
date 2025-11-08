// Test basic stack instrumentation functionality
// This test should pass without errors when run with -i flag

int main() {
    int x = 10;
    int y = 20;

    {
        int z = 30;
        x = x + z;  // Access outer scope variable
    }
    // z is now out of scope

    int result = x + y;  // Should be 40 + 20 = 60

    return result - 60;  // Should return 0
}
