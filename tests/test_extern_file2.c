// File 2: Uses extern declarations
#include "test_extern_header.h"

int use_shared() {
    return shared_var * 2;
}

int call_helper(int val) {
    return helper_func(val);
}

int main() {
    // shared_var is 42 (from file1)
    int result1 = use_shared();  // Should return 84
    
    // helper_func(10) = 10 + 42 = 52
    int result2 = call_helper(10);  // Should return 52
    
    // Verify results
    if (result1 == 84 && result2 == 52) {
        return 0;  // Success
    }
    
    return 1;  // Failure
}
