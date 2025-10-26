// File 1: Definitions
#include "test_extern_header.h"

// Define the shared variable
int shared_var = 42;

// Define the helper function
int helper_func(int x) {
    return x + shared_var;
}
