// Test array decay to pointer in various contexts
// Expected return: 42

// Test 1: Array parameter decays to pointer
int sum_array(int arr[], int len) {
    int sum = 0;
    int i = 0;
    while (i < len) {
        sum = sum + arr[i];
        i = i + 1;
    }
    return sum;
}

// Test 2: Alternative syntax - explicit pointer parameter
int sum_ptr(int *ptr, int len) {
    int sum = 0;
    int i = 0;
    while (i < len) {
        sum = sum + ptr[i];
        i = i + 1;
    }
    return sum;
}

// Test 3: Array decays when passed to function
int get_first(int arr[]) {
    return arr[0];
}

// Test 4: Multidimensional array partial decay
// int arr[3][2] decays to int (*)[2] (pointer to array of 2 ints)
int sum_2d_row(int row[2]) {
    return row[0] + row[1];
}

// Test 5: sizeof behaves differently for array vs decayed pointer
int check_array_size(int arr[5]) {
    // Inside function, arr is a pointer, so sizeof(arr) would be pointer size
    // But we can still access elements
    return arr[0] + arr[1] + arr[2] + arr[3] + arr[4];
}

int main() {
    // Test 1: Array argument decays to pointer
    int arr1[5];
    arr1[0] = 1;
    arr1[1] = 2;
    arr1[2] = 3;
    arr1[3] = 4;
    arr1[4] = 5;
    
    int sum1 = sum_array(arr1, 5);  // 1+2+3+4+5 = 15
    if (sum1 != 15) return 1;
    
    // Test 2: Same array with explicit pointer parameter
    int sum2 = sum_ptr(arr1, 5);    // 15
    if (sum2 != 15) return 2;
    
    // Test 3: Array decay in assignment
    int *ptr = arr1;  // arr1 decays to pointer
    if (ptr[0] != 1) return 3;
    if (ptr[4] != 5) return 4;
    
    // Test 4: Array name in expression context decays to pointer
    int val = get_first(arr1);      // arr1 decays to pointer
    if (val != 1) return 5;
    
    // Test 5: Passing array subset (pointer arithmetic)
    int val2 = get_first(arr1 + 2); // arr1+2 points to arr1[2]
    if (val2 != 3) return 6;
    
    // Test 6: Multidimensional array
    int arr2[3][2];
    arr2[0][0] = 10;
    arr2[0][1] = 15;
    arr2[1][0] = 5;
    arr2[1][1] = 12;
    
    int row_sum = sum_2d_row(arr2[0]);  // Pass first row
    if (row_sum != 25) return 7;
    
    row_sum = sum_2d_row(arr2[1]);      // Pass second row
    if (row_sum != 17) return 8;
    
    // Test 7: sizeof distinction
    // sizeof(arr1) in main gives full array size
    // but inside function it's pointer size
    int arr3[5];
    arr3[0] = 2;
    arr3[1] = 4;
    arr3[2] = 6;
    arr3[3] = 8;
    arr3[4] = 10;
    
    int sum3 = check_array_size(arr3);  // 2+4+6+8+10 = 30
    if (sum3 != 30) return 9;
    
    // Test 8: Array doesn't decay with address-of operator
    int (*ptr_to_arr)[5] = &arr1;  // Pointer to entire array
    int first_elem = (*ptr_to_arr)[0];
    if (first_elem != 1) return 10;
    
    // Test 9: String literals are arrays that decay
    char *str = "Hello";  // String literal decays to char*
    if (str[0] != 'H') return 11;
    if (str[1] != 'e') return 12;
    
    return 42;
}
