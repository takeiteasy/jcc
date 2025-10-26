// Simple heap test - just allocate and free
void *malloc(unsigned long size);
void free(void *ptr);

int main() {
    char *buffer;

    buffer = (char *)malloc(16);
    if (!buffer) return 1;

    buffer[0] = 'A';
    buffer[15] = 'B';
    free(buffer);

    return 0;
}
