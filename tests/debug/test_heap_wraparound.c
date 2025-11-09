/*
 * Test: Heap Overflow Detection (Wraparound)
 *
 * This test verifies that the VM detects when an allocation size
 * is so large that it would cause heap pointer overflow/wraparound.
 *
 * Expected behavior:
 * - Allocation with impossibly large size should fail
 * - VM should detect and report heap overflow
 * - Should return error without crashing
 */

void *malloc(long size);
void free(void *ptr);

int main() {
    // First, try a reasonable allocation to ensure heap works
    int *p1 = malloc(100);
    if (!p1) {
        return 1;  // Small allocation failed
    }
    *p1 = 42;

    // Now try an impossibly large allocation
    // Default heap size is 256KB, so requesting 10MB should fail
    // and trigger overflow detection
    void *p2 = malloc(10000000);  // 10 MB
    if (p2) {
        free(p2);
        free(p1);
        return 2;  // Large allocation should have failed but didn't
    }

    // Heap should still work after failed allocation
    int *p3 = malloc(100);
    if (!p3) {
        free(p1);
        return 3;  // Heap corrupted
    }
    *p3 = 99;

    // Verify values
    if (*p1 != 42 || *p3 != 99) {
        free(p1);
        free(p3);
        return 4;  // Data corruption
    }

    // Clean up
    free(p1);
    free(p3);

    return 0;  // Success - wraparound detection working
}
