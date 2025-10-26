// Comprehensive test of VLA enhancements
// Expected return: 42

int test_vla_cleanup() {
    // Test automatic cleanup on return
    int n = 10;
    int arr[n];
    arr[0] = 42;
    return arr[0];  // VLA cleaned up automatically
}

int test_multiple_returns() {
    int n = 5;
    int arr[n];
    
    arr[0] = 10;
    
    if (arr[0] == 10) {
        return 42;  // VLA cleaned up here
    }
    
    return 0;  // VLA would be cleaned up here too
}

int main() {
    int r1 = test_vla_cleanup();
    if (r1 != 42) return 1;
    
    int r2 = test_multiple_returns();
    if (r2 != 42) return 2;
    
    return 42;  // Success
}
