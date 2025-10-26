// Simple bounds check test
void *malloc(unsigned long size);

int main() {
    char *ptr = (char *)malloc(10);  // Allocate 10 bytes
    if (!ptr) return 1;

    // Access just past the end
    char c = ptr[10];  // 10 bytes allocated, so valid indices are 0-9

    return c;
}
