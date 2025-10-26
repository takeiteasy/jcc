// Most basic malloc test
void *malloc(unsigned long size);

int main() {
    void *ptr = malloc(8);
    return ptr ? 0 : 1;
}
