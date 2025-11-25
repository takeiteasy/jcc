// Minimal test for global array
int arr[3] = {10, 20, 30};

int main() {
    int val = arr[0];  // Should return 10
    if (val != 10) return 1;  // Assert val == 10
    return 42;
}
