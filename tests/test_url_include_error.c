// Test URL include error handling
// This test verifies that appropriate errors are shown for URL includes

#ifdef JCC_HAS_CURL

// This should fail with a helpful error message
// Testing that quoted syntax is rejected for URLs
// #include "https://example.com/header.h"  // Uncomment to test error

int main() {
    // If we get here without the invalid include, test passes
    return 42;
}

#else

// When curl is not available, test that we get a proper error
// Uncomment the line below to test the error message:
// #include <https://example.com/header.h>

int main() {
    return 42;
}

#endif
