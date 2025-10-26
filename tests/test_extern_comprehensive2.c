// Comprehensive extern test - definitions
// This file provides the definitions for extern declarations

// Define the global variable
int global_counter = 0;

// Define the array
int array[5] = {1, 2, 3, 4, 5};

// Define the increment function
void increment_counter() {
    global_counter = global_counter + 1;
}

// Define the array sum function
int get_array_sum() {
    int sum = 0;
    for (int i = 0; i < 5; i = i + 1) {
        sum = sum + array[i];
    }
    return sum;
}
