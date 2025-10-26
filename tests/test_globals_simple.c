// Simple test for global variables (without arrays)

int global_var = 42;
int uninit_global;

int get_global() {
    return global_var;
}

int main() {
    // Test initialized global
    if (get_global() != 42) return 1;
    
    // Test uninitialized global (should be 0)
    if (uninit_global != 0) return 2;
    
    // Modify uninitialized global
    uninit_global = 100;
    if (uninit_global != 100) return 3;
    
    // Modify initialized global
    global_var = 10;
    if (global_var != 10) return 4;
    
    return 42;  // Success
}
