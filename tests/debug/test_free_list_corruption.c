/*
 * Test: Free List Corruption Detection
 *
 * This test verifies that the VM detects corrupted free list entries.
 * We can't directly corrupt the free list without VM internals access,
 * but we can test scenarios that might lead to corruption and verify
 * the VM handles them gracefully.
 *
 * Expected behavior:
 * - VM should validate free blocks before using them
 * - Should detect blocks with invalid sizes
 * - Should handle edge cases without crashing
 */

void *malloc(long size);
void free(void *ptr);

// Test pattern: Allocate and free in patterns that stress the free list
int main() {
    int i;
    long ptrs[100];

    // Test 1: Allocate many blocks of varying sizes
    for (i = 0; i < 20; i++) {
        ptrs[i] = (long)malloc((i + 1) * 8);
        if (!ptrs[i]) {
            return 1;  // Allocation failed
        }
    }

    // Test 2: Free every other block to fragment the heap
    for (i = 0; i < 20; i = i + 2) {
        free((void *)ptrs[i]);
        ptrs[i] = 0;
    }

    // Test 3: Reallocate in the free gaps
    for (i = 0; i < 20; i = i + 2) {
        ptrs[i] = (long)malloc((i + 1) * 8);
        if (!ptrs[i]) {
            return 2;  // Reallocation failed
        }
    }

    // Test 4: Free all blocks
    for (i = 0; i < 20; i++) {
        if (ptrs[i]) {
            free((void *)ptrs[i]);
        }
    }

    // Test 5: Allocate after mass free (tests coalescing and free list integrity)
    long large = (long)malloc(5000);
    if (!large) {
        return 3;  // Large allocation failed
    }

    // Test 6: Verify heap still functional
    for (i = 0; i < 10; i++) {
        ptrs[i] = (long)malloc(64);
        if (!ptrs[i]) {
            free((void *)large);
            return 4;  // Small allocation failed
        }
        // Write pattern to verify no corruption
        int *p = (int *)ptrs[i];
        *p = i * 100;
    }

    // Verify patterns
    for (i = 0; i < 10; i++) {
        int *p = (int *)ptrs[i];
        if (*p != i * 100) {
            return 5;  // Data corruption detected
        }
    }

    // Clean up
    free((void *)large);
    for (i = 0; i < 10; i++) {
        free((void *)ptrs[i]);
    }

    return 0;  // Success
}
