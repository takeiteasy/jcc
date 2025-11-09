void *malloc(long size);
void free(void *ptr);

int main() {
    int i;
    long ptrs[20];

    // Allocate 15 blocks
    for (i = 0; i < 15; i++) {
        ptrs[i] = (long)malloc((i + 1) * 8);
        if (!ptrs[i]) {
            return 100 + i;
        }
    }

    // Free every other block
    for (i = 0; i < 15; i = i + 2) {
        free((void *)ptrs[i]);
        ptrs[i] = 0;
    }

    // Reallocate in the gaps
    for (i = 0; i < 15; i = i + 2) {
        ptrs[i] = (long)malloc((i + 1) * 8);
        if (!ptrs[i]) {
            return 200 + i;
        }
    }

    // Free all
    for (i = 0; i < 15; i++) {
        free((void *)ptrs[i]);
    }

    return 0;  // Success
}
