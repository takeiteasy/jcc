// Test: Minimal VLA test
// Expected return: 42

int main() {
    int n = 1;
    int arr[n];
    arr[0] = 42;
    return arr[0];
}
