void *malloc(long size);
void free(void *ptr);

int main() {
    int i;
    long ptrs[20];

    // Allocate blocks
    for (i = 0; i < 10; i++) {
        ptrs[i] = (long)malloc((i + 1) * 16);
        if (!ptrs[i]) {
            return 1;
        }
    }

    // Free all blocks
    for (i = 0; i < 10; i++) {
        free((void *)ptrs[i]);
    }

    // Allocate again
    for (i = 0; i < 10; i++) {
        ptrs[i] = (long)malloc((i + 1) * 16);
        if (!ptrs[i]) {
            return 2;
        }
    }

    // Cleanup
    for (i = 0; i < 10; i++) {
        free((void *)ptrs[i]);
    }

    return 0;
}
