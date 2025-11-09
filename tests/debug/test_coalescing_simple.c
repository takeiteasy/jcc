// Test memory coalescing: verify that adjacent freed blocks are merged

void *malloc(long long size);
void free(void *ptr);

int main() {
    // Allocate three adjacent 100-byte blocks
    int *a = (int*)malloc(100);
    int *b = (int*)malloc(100);
    int *c = (int*)malloc(100);

    if (!a || !b || !c) {
        return 1;  // Initial allocation failed
    }

    // Free first and third blocks (creates fragmentation)
    free(a);
    free(c);

    // Free middle block (should coalesce all three if coalescing works)
    free(b);

    // Try to allocate a larger block (300 bytes)
    // This should succeed if coalescing merged the three 100-byte blocks
    // Without coalescing, this would fail because we'd have three separate 100-byte blocks
    int *big = (int*)malloc(300);

    if (!big) {
        return 2;  // Large allocation failed - coalescing not working
    }

    free(big);

    return 0;  // Success! Coalescing works
}
