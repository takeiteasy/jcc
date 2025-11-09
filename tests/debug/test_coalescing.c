// Test memory coalescing: verify that adjacent freed blocks are merged

void *malloc(long long size);
void free(void *ptr);
int printf(char *fmt, ...);

int main() {
    // Allocate three adjacent 100-byte blocks
    int *a = (int*)malloc(100);
    int *b = (int*)malloc(100);
    int *c = (int*)malloc(100);

    if (!a || !b || !c) {
        printf("Initial allocation failed\n");
        return 1;
    }

    printf("Allocated a=%lld, b=%lld, c=%lld\n", (long long)a, (long long)b, (long long)c);

    // Free first and third blocks (creates fragmentation)
    free(a);
    free(c);
    printf("Freed a and c (fragmented)\n");

    // Free middle block (should coalesce all three if coalescing works)
    free(b);
    printf("Freed b (should coalesce)\n");

    // Try to allocate a larger block (300 bytes)
    // This should succeed if coalescing merged the three 100-byte blocks
    // Without coalescing, this would fail because we'd have three separate 100-byte blocks
    int *big = (int*)malloc(300);

    if (!big) {
        printf("Large allocation failed - coalescing may not be working\n");
        return 2;
    }

    printf("Large allocation succeeded at %lld - coalescing works!\n", (long long)big);

    free(big);

    return 0;
}
