// Comprehensive test demonstrating extern keyword functionality
// This test uses multiple files to demonstrate external linkage

// Test 1: Simple extern variable
extern int x;

int main() {
    // x is defined in test_extern_simple2.c as 42
    if (x != 42) return 1;
    
    return 0;  // Success
}
