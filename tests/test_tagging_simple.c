void *malloc(long size);

int main() {
    int *ptr = (int *)malloc(sizeof(int));
    *ptr = 42;
    return *ptr;
}
