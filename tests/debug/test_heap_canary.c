// Test for heap canary detection
// This should trigger a heap overflow error when -heap-canaries is used

void *malloc(unsigned long size);
void free(void *ptr);

int main() {
    char *buffer;
    char *guard;  // Allocate another buffer after to protect heap structures

    // Allocate 16 bytes
    buffer = (char *)malloc(16);
    if (!buffer) return 1;

    // Allocate a guard buffer to prevent corruption of heap structures
    guard = (char *)malloc(64);
    if (!guard) return 1;

    // Write just past the end to corrupt the rear canary
    // Buffer is 16 bytes (buffer[0-15]), so buffer[16-23] is the rear canary
    buffer[16] = 'X';
    buffer[17] = 'Y';
    buffer[18] = 'Z';
    buffer[19] = 'W';
    buffer[20] = 'Q';
    buffer[21] = 'R';
    buffer[22] = 'S';
    buffer[23] = 'T';

    // Free will detect the corrupted canary
    free(buffer);
    free(guard);

    return 0;
}
