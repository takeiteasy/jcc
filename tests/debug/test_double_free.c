// Test: Double-free detection
// Expected: Should detect double-free and abort with error message

void *malloc(unsigned long size);
void free(void *ptr);

int main() {
    void *ptr = malloc(100);

    // First free - should succeed
    free(ptr);

    // Second free - should detect double-free and abort
    free(ptr);

    return 0;  // Should never reach here
}
