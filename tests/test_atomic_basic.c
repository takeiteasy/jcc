/*
 * Basic Atomic Operations Test
 * Tests low-level __jcc_atomic_* builtins directly
 */

int main() {
    // Test 1: Atomic exchange
    int x = 42;
    int old = __jcc_atomic_exchange(&x, 100);

    if (old != 42) return 1;  // Old value should be 42
    if (x != 100) return 2;   // New value should be 100

    // Test 2: Compare-and-swap (success case)
    int y = 10;
    int expected = 10;
    int result = __jcc_compare_and_swap(&y, &expected, 20);

    if (result != 1) return 3;  // Should succeed (return 1)
    if (y != 20) return 4;       // Value should be updated to 20

    // Test 3: Compare-and-swap (failure case)
    int z = 5;
    int wrong_expected = 99;  // Wrong expected value
    result = __jcc_compare_and_swap(&z, &wrong_expected, 50);

    if (result != 0) return 5;  // Should fail (return 0)
    if (z != 5) return 6;        // Value should remain unchanged

    // Test 4: Multiple exchanges
    long val = 1000;
    long prev = __jcc_atomic_exchange(&val, 2000);
    if (prev != 1000) return 7;
    if (val != 2000) return 8;

    prev = __jcc_atomic_exchange(&val, 3000);
    if (prev != 2000) return 9;
    if (val != 3000) return 10;

    // Test 5: CAS in a loop (simulate retry)
    int counter = 0;
    for (int i = 0; i < 10; i++) {
        int old_val = counter;
        int new_val = old_val + 1;
        while (!__jcc_compare_and_swap(&counter, &old_val, new_val)) {
            old_val = counter;
            new_val = old_val + 1;
        }
    }
    if (counter != 10) return 11;

    return 0;  // All tests passed
}
