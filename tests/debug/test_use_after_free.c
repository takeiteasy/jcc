// Test use-after-free detection
// Should detect accessing freed memory when -uaf-detection is used

void *malloc(unsigned long size);
void free(void *ptr);

int main() {
    int *ptr;

    // Allocate some memory
    ptr = (int *)malloc(sizeof(int) * 10);
    if (!ptr) return 1;

    // Use it validly
    ptr[0] = 42;
    ptr[5] = 100;

    // Free it
    free(ptr);

    // Try to access it after free - should trigger UAF error
    int value = ptr[0];  // Use-after-free!

    return value;
}
