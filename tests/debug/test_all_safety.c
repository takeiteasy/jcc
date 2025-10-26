// Comprehensive test for all Phase 1 & 2 memory safety features
// Run with different -* flags to test each feature

void *malloc(unsigned long size);
void free(void *ptr);

int test_stack_overflow() {
    char buffer[8];
    // Write past buffer to corrupt stack canary
    buffer[8] = 'A';
    buffer[9] = 'B';
    buffer[10] = 'C';
    buffer[11] = 'D';
    buffer[12] = 'E';
    buffer[13] = 'F';
    buffer[14] = 'G';
    buffer[15] = 'H';
    return 0;
}

int test_heap_overflow() {
    char *buf = (char *)malloc(16);
    if (!buf) return 1;

    // Corrupt rear canary
    buf[16] = 'X';
    buf[17] = 'Y';
    buf[18] = 'Z';
    buf[19] = 'W';
    buf[20] = 'Q';
    buf[21] = 'R';
    buf[22] = 'S';
    buf[23] = 'T';

    free(buf);
    return 0;
}

int test_memory_leak() {
    char *leak1 = (char *)malloc(100);
    char *leak2 = (char *)malloc(200);
    char *no_leak = (char *)malloc(50);
    free(no_leak);
    // leak1 and leak2 are not freed
    return 0;
}

int test_use_after_free() {
    int *ptr = (int *)malloc(sizeof(int) * 10);
    if (!ptr) return 1;

    ptr[0] = 42;
    free(ptr);

    // Use after free!
    int value = ptr[0];
    return value;
}

int test_bounds_check() {
    char *arr = (char *)malloc(10);
    if (!arr) return 1;

    // Out of bounds access
    char c = arr[10];

    free(arr);
    return c;
}

int main() {
    // Uncomment one test at a time
    // return test_stack_overflow();
    // return test_heap_overflow();
    // return test_memory_leak();
    // return test_use_after_free();
    return test_bounds_check();
}
