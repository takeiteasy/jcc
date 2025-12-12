// Test array parameter decay
int sum_array(int arr[], int len) {
    int sum = 0;
    int i = 0;
    while (i < len) {
        sum = sum + arr[i];
        i = i + 1;
    }
    return sum;
}

int main() {
    int arr[5];
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;
    arr[4] = 5;
    int result = sum_array(arr, 5);
    if (result != 15) return 1;
    return 42;
}
