// Test: VLA basic allocation
// Expected return: 42

int main() {
    int n = 3;
    int arr[n];
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 12;
    if (arr[0] + arr[1] + arr[2] != 42) return 1;
    return 42;
}
