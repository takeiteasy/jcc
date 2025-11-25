// Comprehensive extern test
// This file demonstrates all extern use cases

// Extern variable declarations (defined in test_extern_comprehensive2.c)
extern int global_counter;

// Extern function declarations (defined in test_extern_comprehensive2.c)
extern void increment_counter();

int main() {
    // global_counter starts at 0
    increment_counter();
    increment_counter();

    // Should be 2 now
    if (global_counter != 2) return global_counter;  // Return actual value for debugging

    return 42;  // Success
}
