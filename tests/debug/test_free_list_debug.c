void *malloc(long size);
void free(void *ptr);

int main() {
    int i;
    long ptrs[100];

    // Test 1: Allocate many blocks of varying sizes
    for (i = 0; i < 20; i++) {
        ptrs[i] = (long)malloc((i + 1) * 8);
        if (!ptrs[i]) {
            return 100 + i;  // Return unique code showing which allocation failed
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
            return 200 + i;  // Unique code for reallocation failure
        }
    }

    // Test 4: Free all blocks
    for (i = 0; i < 20; i++) {
        if (ptrs[i]) {
            free((void *)ptrs[i]);
        }
    }

    // Made it past mass free/alloc
    return 10;  // Checkpoint: got past block tests
}
