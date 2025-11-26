// Comprehensive extern test (merged with test_extern_comprehensive2.c)
// This file demonstrates all extern use cases

// Definitions
int global_counter = 0;
int array[5] = {1, 2, 3, 4, 5};

void increment_counter() {
    global_counter = global_counter + 1;
}

int get_array_sum() {
    int sum = 0;
    for (int i = 0; i < 5; i = i + 1) {
        sum = sum + array[i];
    }
    return sum;
}

int main() {
    // global_counter starts at 0
    increment_counter();
    increment_counter();

    // Should be 2 now
    if (global_counter != 2) return 1;

    return 42;  // Success
}
