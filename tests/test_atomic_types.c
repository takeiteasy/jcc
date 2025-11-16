/*
 * Atomic Types Test
 * Tests different atomic type sizes and pointer atomics
 */

#include <stdatomic.h>

int main() {
    // Test 1: atomic_int
    atomic_int ai = ATOMIC_VAR_INIT(42);
    int old_i = atomic_exchange(&ai, 100);
    if (old_i != 42) return 1;
    if (atomic_load(&ai) != 100) return 2;

    // Test 2: atomic_long
    atomic_long al = ATOMIC_VAR_INIT(1000);
    long old_l = atomic_exchange(&al, 2000);
    if (old_l != 1000) return 3;
    if (atomic_load(&al) != 2000) return 4;

    // Test 3: atomic_store and atomic_load
    atomic_int stored = ATOMIC_VAR_INIT(0);
    atomic_store(&stored, 555);
    if (atomic_load(&stored) != 555) return 5;

    // Test 4: atomic_compare_exchange_strong
    atomic_int cmp = ATOMIC_VAR_INIT(10);
    int expected = 10;
    int success = atomic_compare_exchange_strong(&cmp, &expected, 20);
    if (!success) return 6;
    if (atomic_load(&cmp) != 20) return 7;

    // Test 5: atomic_compare_exchange_strong failure
    atomic_int cmp2 = ATOMIC_VAR_INIT(5);
    int wrong = 999;
    success = atomic_compare_exchange_strong(&cmp2, &wrong, 50);
    if (success) return 8;  // Should fail
    if (atomic_load(&cmp2) != 5) return 9;  // Value unchanged

    // Test 6: atomic_fetch_add
    atomic_int fetch = ATOMIC_VAR_INIT(100);
    int prev = atomic_fetch_add(&fetch, 50);
    if (prev != 100) return 10;
    if (atomic_load(&fetch) != 150) return 11;

    // Test 7: atomic_fetch_sub
    prev = atomic_fetch_sub(&fetch, 30);
    if (prev != 150) return 12;
    if (atomic_load(&fetch) != 120) return 13;

    // Test 8: atomic_flag
    atomic_flag flag = ATOMIC_FLAG_INIT;
    int was_set = atomic_flag_test_and_set(&flag);
    if (was_set != 0) return 14;  // Initially cleared

    was_set = atomic_flag_test_and_set(&flag);
    if (was_set != 1) return 15;  // Now set

    atomic_flag_clear(&flag);
    was_set = atomic_flag_test_and_set(&flag);
    if (was_set != 0) return 16;  // Cleared again

    // Test 9: Atomic pointer (cast as long long)
    long long dummy_var = 0xDEADBEEF;
    _Atomic long long ptr_val = (long long)&dummy_var;
    long long old_ptr = atomic_exchange(&ptr_val, (long long)0);
    if (old_ptr != (long long)&dummy_var) return 17;
    if (atomic_load(&ptr_val) != 0) return 18;

    // Test 10: Multiple operations
    atomic_int multi = ATOMIC_VAR_INIT(10);
    atomic_fetch_add(&multi, 5);   // 15
    atomic_fetch_sub(&multi, 3);   // 12
    atomic_fetch_add(&multi, 8);   // 20
    if (atomic_load(&multi) != 20) return 19;

    return 0;  // All tests passed
}
