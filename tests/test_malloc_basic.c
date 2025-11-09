// Simple malloc/free test without coalescing
int malloc(int size);
void free(void *ptr);

int main() {
    void *p1 = malloc(100);
    if (p1 == 0) return 1;

    free(p1);

    void *p2 = malloc(100);
    if (p2 == 0) return 2;

    free(p2);
    return 0;
}
