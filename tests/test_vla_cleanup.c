// Test VLA memory cleanup on function exit
// Expected return: 42
// This test verifies that VLAs are properly freed when functions return

int test_vla_in_function() {
    int n = 10;
    int arr[n];
    
    // Use the array
    arr[0] = 42;
    int result = arr[0];
    
    // VLA should be freed automatically when function returns
    return result;
}

int test_multiple_vlas() {
    int n1 = 5;
    int n2 = 10;
    
    int arr1[n1];
    int arr2[n2];
    
    arr1[0] = 20;
    arr2[0] = 22;
    
    int result = arr1[0] + arr2[0];  // 20 + 22 = 42
    
    // Both VLAs should be freed automatically
    return result;
}

int test_vla_in_nested_scopes() {
    int result = 0;
    
    {
        int n = 5;
        int arr[n];
        arr[0] = 10;
        result += arr[0];
    }
    
    {
        int n = 8;
        int arr[n];
        arr[0] = 32;
        result += arr[0];
    }
    
    return result;  // 10 + 32 = 42
}

int main() {
    int r1 = test_vla_in_function();
    if (r1 != 42) return 1;
    
    int r2 = test_multiple_vlas();
    if (r2 != 42) return 2;
    
    int r3 = test_vla_in_nested_scopes();
    if (r3 != 42) return 3;
    
    return 42;  // Success
}
