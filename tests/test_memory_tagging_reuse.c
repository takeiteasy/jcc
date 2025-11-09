// Test temporal memory tagging with explicit memory reuse
// This should FAIL when run with --memory-tagging flag
// Expected: TEMPORAL SAFETY VIOLATION error

void *malloc(long size);
void free(void *ptr);

struct Data {
    int value;
    int count;
};

int main() {
    // Allocate and use first object
    struct Data *data1 = (struct Data *)malloc(sizeof(struct Data));
    data1->value = 42;
    data1->count = 1;

    // Save pointer to first allocation
    struct Data *stale = data1;

    // Free first object
    free(data1);

    // Allocate second object (will reuse same memory)
    struct Data *data2 = (struct Data *)malloc(sizeof(struct Data));
    data2->value = 100;
    data2->count = 2;

    // Now stale pointer points to reused memory
    // Access through stale pointer should trigger temporal safety violation
    // because stale was tagged with generation N, but memory is now generation N+1
    int bad_value = stale->value;

    // This should never execute
    return bad_value;
}
