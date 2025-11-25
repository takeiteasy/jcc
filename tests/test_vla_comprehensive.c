// Test: Comprehensive VLA test with multiple scenarios
// Expected return: 100

int sum_array(int *arr, int size) {
    int sum = 0;
    for (int i = 0; i < size; i = i + 1) {
        sum = sum + arr[i];
    }
    return sum;
}

int main() {
    int n = 5;
    int arr[n];
    
    // Initialize array
    for (int i = 0; i < n; i = i + 1) {
        arr[i] = (i + 1) * 10;  // 10, 20, 30, 40, 50
    }
    
    // Test reading values
    int test1 = arr[0];  // 10
    int test2 = arr[4];  // 50
    
    // Test pointer arithmetic
    int *ptr = arr;
    int test3 = ptr[2];  // 30
    
    // Test passing VLA to function
    int total = sum_array(arr, n);  // 10+20+30+40+50 = 150
    
    // Verify results
    if (test1 != 10) return 1;
    if (test2 != 50) return 2;
    if (test3 != 30) return 3;
    if (total != 150) return 4;

    return 42;  // Success
}
