// Comprehensive test for global variables

// Initialized globals
int global_int = 100;
char global_char = 'X';

// Uninitialized globals
int uninit_global;
char uninit_char;

// Array globals
int global_array[5] = {1, 2, 3, 4, 5};

// Test reading initialized global
int read_global_int() {
    return global_int;
}

// Test writing to global
void write_global_int(int val) {
    global_int = val;
}

// Test reading and writing char global
char get_and_set_char(char new_val) {
    char old = global_char;
    global_char = new_val;
    return old;
}

// Test uninitialized global (should be zero-initialized)
int test_uninit() {
    // Uninitialized globals should be zero
    if (uninit_global != 0) return 1;
    if (uninit_char != 0) return 2;
    
    // Write to them
    uninit_global = 50;
    uninit_char = 'Y';
    
    // Read back
    if (uninit_global != 50) return 3;
    if (uninit_char != 'Y') return 4;
    
    return 0;  // Success
}

// Test global array access
int test_global_array() {
    // Read values
    if (global_array[0] != 1) return 1;
    if (global_array[4] != 5) return 2;
    
    // Modify array
    global_array[2] = 99;
    
    // Read back
    if (global_array[2] != 99) return 3;
    
    return 0;  // Success
}

// Test pointer to global
int test_global_pointer() {
    int *ptr = &global_int;
    *ptr = 200;
    
    if (global_int != 200) return 1;
    
    return 0;  // Success
}

int main() {
    int errors = 0;
    
    // Test 1: Read initialized global
    if (read_global_int() != 100) errors++;
    
    // Test 2: Write and read back
    write_global_int(150);
    if (read_global_int() != 150) errors++;
    
    // Test 3: Char global
    char old_char = get_and_set_char('Z');
    if (old_char != 'X') errors++;
    if (global_char != 'Z') errors++;
    
    // Test 4: Uninitialized globals
    if (test_uninit() != 0) errors++;
    
    // Test 5: Global arrays
    if (test_global_array() != 0) errors++;
    
    // Test 6: Pointers to globals
    if (test_global_pointer() != 0) errors++;
    
    // If no errors, return success code
    if (errors == 0) {
        return 42;
    }
    
    return errors;
}
