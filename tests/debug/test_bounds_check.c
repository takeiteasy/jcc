// Test bounds checking
// Should detect out-of-bounds array access when -bounds-checks is used

void *malloc(unsigned long size);
void free(void *ptr);

int main() {
    int *arr;
    int i;

    // Allocate array of 10 ints
    arr = (int *)malloc(10 * sizeof(int));
    if (!arr) return 1;

    // Valid accesses
    for (i = 0; i < 10; i++) {
        arr[i] = i * 10;
    }

    // Out of bounds access - should trigger bounds error
    int value = arr[100];  // Way past the end!

    free(arr);
    return value;
}
