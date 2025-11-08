// test_alignment.c
// Tests pointer alignment checking
// Expected behavior: Should detect misaligned pointer access

void *malloc(unsigned long size);

int main() {
    // Allocate a buffer and create a misaligned pointer
    char *buffer = (char *)malloc(16);

    // Create a misaligned int pointer (offset by 1 byte)
    int *misaligned = (int *)(buffer + 1);

    // This should trigger an alignment error
    int value = *misaligned;

    return value;
}
