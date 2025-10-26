// Test global variables

int global_var = 42;
int uninitialized_global;

int get_global() {
    return global_var;
}

int set_global(int val) {
    global_var = val;
    return global_var;
}

int test_uninitialized() {
    uninitialized_global = 100;
    return uninitialized_global;
}

int main() {
    // Test initialized global
    int result = get_global();  // Should return 42
    
    // Test global assignment
    set_global(10);
    result = get_global();  // Should return 10
    
    // Test uninitialized global
    int uninit_result = test_uninitialized();  // Should return 100
    
    // Verify final state
    if (get_global() == 10 && uninit_result == 100) {
        return 42;  // Success
    }
    
    return 1;  // Failure
}
