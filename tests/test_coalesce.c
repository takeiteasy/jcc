// Test free block coalescing
// Allocates several blocks, frees them in a pattern that creates
// fragmentation, then verifies coalescing merges adjacent blocks

void *malloc(int size);
void free(void *ptr);

int main() {
    // Allocate 5 blocks of 100 bytes each
    void *p1 = malloc(100);
    void *p2 = malloc(100);
    void *p3 = malloc(100);
    void *p4 = malloc(100);
    void *p5 = malloc(100);

    // Free them in a pattern: free 1, 3, 5 first
    // This creates fragmentation with holes
    free(p1);
    free(p3);
    free(p5);

    // Now free 2 and 4
    // Coalescing should merge adjacent blocks:
    // - p1 and p2 should merge
    // - p3 and p4 should merge
    free(p2);
    free(p4);

    // If coalescing works, we should be able to allocate
    // a large block that spans multiple original blocks
    void *large = malloc(400);
    if (large == 0) {
        return 1;  // Failed - fragmentation prevented allocation
    }

    free(large);
    return 42;  // Success
}
