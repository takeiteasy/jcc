// Test global arrays

int global_array[5] = {10, 20, 30, 40, 50};

int main() {
    // Test reading array elements
    if (global_array[0] != 10) return 1;
    if (global_array[1] != 20) return 2;
    if (global_array[4] != 50) return 3;
    
    // Test modifying array elements
    global_array[2] = 99;
    if (global_array[2] != 99) return 4;
    
    return 42;  // Success
}
