#include <stdio.h>

void *malloc(long size);
void free(void *ptr);
void *realloc(void *ptr, long size);
void *calloc(long count, long size);

int main() {
    // Test calloc
    int *arr = (int *)calloc(3, sizeof(int));
    if (!arr) {
        printf("FAIL: calloc returned NULL\n");
        return 1;
    }

    // Check zeroed
    if (arr[0] != 0 || arr[1] != 0 || arr[2] != 0) {
        printf("FAIL: calloc not zeroed\n");
        return 1;
    }

    printf("PASS: calloc works\n");

    // Set values
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;

    // Test realloc
    int *arr2 = (int *)realloc(arr, 5 * sizeof(int));
    if (!arr2) {
        printf("FAIL: realloc returned NULL\n");
        return 1;
    }

    // Check data preserved
    if (arr2[0] != 10 || arr2[1] != 20 || arr2[2] != 30) {
        printf("FAIL: realloc lost data\n");
        return 1;
    }

    printf("PASS: realloc works\n");

    free(arr2);

    printf("All tests passed!\n");
    return 42;
}
