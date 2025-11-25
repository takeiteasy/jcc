// Test: Ultra minimal - just create VLA, don't use it
// Expected return: 1

int main() {
    int n = 1;
    int arr[n];
    if (n != 1) return 0;  // Assert n == 1
    return 42;
}
