// Test basic temporal memory tagging - UAF with memory reuse
// This should FAIL when run with --memory-tagging flag
// Expected: TEMPORAL SAFETY VIOLATION error

void *malloc(long size);
void free(void *ptr);

int main() {
    // Allocate memory
    int *ptr1 = (int *)malloc(sizeof(int) * 10);
    *ptr1 = 42;

    // Save pointer for later use
    int *stale_ptr = ptr1;

    // Free the memory
    free(ptr1);

    // Allocate new memory (likely reuses the same block)
    // This increments the generation counter for that memory location
    int *ptr2 = (int *)malloc(sizeof(int) * 10);
    *ptr2 = 100;

    // Try to use the stale pointer
    // With memory tagging: stale_ptr has generation 0, but memory now has generation 1
    // This should be caught as a temporal safety violation
    int value = *stale_ptr;

    return value;  // Should never reach here
}
