// TLS array test
_Thread_local int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

int main() {
    // Test initialization
    if (arr[0] != 0) return 1;
    if (arr[9] != 9) return 2;

    // Test write
    arr[5] = 99;
    if (arr[5] != 99) return 3;

    // Test array iteration
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += arr[i];
    }
    // 0+1+2+3+4+99+6+7+8+9 = 139
    if (sum != 139) return 4;

    return 0;  // Success
}
