void *malloc(long size);
void free(void *ptr);

int main() {
    // Simple test: allocate, free, reallocate
    long p1 = (long)malloc(64);
    if (!p1) return 1;

    long p2 = (long)malloc(128);
    if (!p2) return 2;

    long p3 = (long)malloc(64);
    if (!p3) return 3;

    // Free middle block
    free((void *)p2);

    // Reallocate - should reuse p2's slot
    long p4 = (long)malloc(128);
    if (!p4) return 4;

    // Cleanup
    free((void *)p1);
    free((void *)p3);
    free((void *)p4);

    return 0;  // Success
}
