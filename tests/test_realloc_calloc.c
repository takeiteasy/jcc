#include <stdio.h>
// Don't include stdlib.h to avoid atexit issue, declare what we need
void *malloc(long size);
void free(void *ptr);
void *realloc(void *ptr, long size);
void *calloc(long count, long size);

int main() {
    printf("Testing realloc and calloc with --vm-heap\n");

    // Test 1: Basic calloc
    printf("\nTest 1: Basic calloc\n");
    int *arr = (int *)calloc(5, sizeof(int));
    if (!arr) {
        printf("FAIL: calloc returned NULL\n");
        return 1;
    }

    // Verify memory is zeroed
    for (int i = 0; i < 5; i++) {
        if (arr[i] != 0) {
            printf("FAIL: calloc memory not zeroed at index %d (value=%d)\n", i, arr[i]);
            return 1;
        }
    }
    printf("PASS: calloc allocated and zeroed 5 integers\n");

    // Fill with data
    for (int i = 0; i < 5; i++) {
        arr[i] = i * 10;
    }

    // Test 2: realloc to larger size
    printf("\nTest 2: realloc to larger size\n");
    int *arr2 = (int *)realloc(arr, 10 * sizeof(int));
    if (!arr2) {
        printf("FAIL: realloc returned NULL\n");
        return 1;
    }

    // Verify old data is preserved
    for (int i = 0; i < 5; i++) {
        if (arr2[i] != i * 10) {
            printf("FAIL: realloc lost data at index %d (expected=%d, got=%d)\n",
                   i, i * 10, arr2[i]);
            return 1;
        }
    }
    printf("PASS: realloc preserved data when growing\n");

    // Fill new space
    for (int i = 5; i < 10; i++) {
        arr2[i] = i * 10;
    }

    // Test 3: realloc to smaller size
    printf("\nTest 3: realloc to smaller size\n");
    int *arr3 = (int *)realloc(arr2, 3 * sizeof(int));
    if (!arr3) {
        printf("FAIL: realloc to smaller size returned NULL\n");
        return 1;
    }

    // Verify data is preserved
    for (int i = 0; i < 3; i++) {
        if (arr3[i] != i * 10) {
            printf("FAIL: realloc lost data when shrinking at index %d\n", i);
            return 1;
        }
    }
    printf("PASS: realloc preserved data when shrinking\n");

    // Test 4: realloc(NULL, size) == malloc(size)
    printf("\nTest 4: realloc(NULL, size)\n");
    int *arr4 = (int *)realloc(NULL, 4 * sizeof(int));
    if (!arr4) {
        printf("FAIL: realloc(NULL, size) returned NULL\n");
        return 1;
    }
    arr4[0] = 42;
    arr4[1] = 84;
    arr4[2] = 126;
    arr4[3] = 168;
    printf("PASS: realloc(NULL, size) works like malloc\n");

    // Test 5: realloc(ptr, 0) == free(ptr)
    printf("\nTest 5: realloc(ptr, 0)\n");
    int *arr5 = (int *)malloc(sizeof(int));
    *arr5 = 999;
    int *result = (int *)realloc(arr5, 0);
    if (result != NULL) {
        printf("FAIL: realloc(ptr, 0) should return NULL\n");
        return 1;
    }
    printf("PASS: realloc(ptr, 0) works like free\n");

    // Test 6: Large calloc
    printf("\nTest 6: Large calloc\n");
    char *large = (char *)calloc(1000, 1);
    if (!large) {
        printf("FAIL: large calloc returned NULL\n");
        return 1;
    }

    // Check it's zeroed
    int all_zero = 1;
    for (int i = 0; i < 1000; i++) {
        if (large[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    if (!all_zero) {
        printf("FAIL: large calloc memory not zeroed\n");
        return 1;
    }
    printf("PASS: large calloc allocated and zeroed 1000 bytes\n");

    // Test 7: Multiple reallocs in sequence
    printf("\nTest 7: Multiple sequential reallocs\n");
    int *seq = (int *)malloc(sizeof(int));
    *seq = 1;

    for (int size = 2; size <= 8; size++) {
        int *new_seq = (int *)realloc(seq, size * sizeof(int));
        if (!new_seq) {
            printf("FAIL: realloc failed at size %d\n", size);
            return 1;
        }
        seq = new_seq;

        // Verify old data
        for (int i = 0; i < size - 1; i++) {
            if (seq[i] != i + 1) {
                printf("FAIL: data corruption at size %d, index %d\n", size, i);
                return 1;
            }
        }

        // Add new element
        seq[size - 1] = size;
    }
    printf("PASS: multiple sequential reallocs preserved data\n");

    // Cleanup
    free(arr3);
    free(arr4);
    free(large);
    free(seq);

    printf("\n========================================\n");
    printf("All tests passed!\n");
    printf("========================================\n");

    return 42;  // Success indicator
}
