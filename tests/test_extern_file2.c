// Merged extern test (combined file1 and file2)

// Define the shared variable
int shared_var = 42;

// Define the helper function
int helper_func(int x) {
    return x + shared_var;
}

int use_shared() {
    return shared_var * 2;
}

int call_helper(int val) {
    return helper_func(val);
}

int main() {
    // shared_var is 42
    int result1 = use_shared();  // Should return 84

    // helper_func(10) = 10 + 42 = 52
    int result2 = call_helper(10);  // Should return 52

    // Verify results
    if (result1 != 84) return 1;  // Failure
    if (result2 != 52) return 2;  // Failure

    return 42;  // Success
}
