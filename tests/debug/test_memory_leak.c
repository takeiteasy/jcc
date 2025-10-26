// Test for memory leak detection
// This should report memory leaks when -memory-leak-detection is used

void *malloc(unsigned long size);
void free(void *ptr);

int main() {
    char *leak1;
    char *leak2;
    char *no_leak;

    // Allocate but don't free - these will leak
    leak1 = (char *)malloc(100);
    leak2 = (char *)malloc(200);

    // Allocate and free - this won't leak
    no_leak = (char *)malloc(50);
    free(no_leak);

    // Exit without freeing leak1 and leak2
    return 0;
}
