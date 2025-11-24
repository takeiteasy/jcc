// Test zero-length arrays (GNU extension)

struct flexible {
    int count;
    int data[0];  // Zero-length array at end
};

struct with_zero {
    int arr[0];
    int x;
};

int main() {
    // Test sizeof with zero-length array
    unsigned long sz1 = sizeof(struct flexible);
    unsigned long sz2 = sizeof(int);
    if (sz1 != sz2)
        return 1;

    // Test zero-length array in middle
    if (sizeof(struct with_zero) != sizeof(int))
        return 2;

    // Test local zero-length array
    int local_arr[0];
    (void)local_arr;  // Suppress unused warning

    return 42;
}
