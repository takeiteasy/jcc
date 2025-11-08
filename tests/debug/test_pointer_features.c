// test_pointer_features.c
// Comprehensive test demonstrating all pointer sanitizer features
// This test should pass with all checks enabled

void *malloc(unsigned long size);
void free(void *ptr);

int main() {
    // Test 1: Normal pointer usage (should work)
    int x = 42;
    int *p = &x;
    int value = *p;  // Should be fine - pointer is still valid

    // Test 2: Heap allocation (should work)
    int *heap_ptr = (int *)malloc(sizeof(int) * 10);
    heap_ptr[0] = 100;
    int heap_value = heap_ptr[0];  // Should be fine
    free(heap_ptr);

    // Return success
    return value + heap_value - 142;  // Should return 0
}
