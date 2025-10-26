// Test VLA with initialization
// Expected return: 42

int main() {
    int n = 5;
    int arr[n] = {10, 20, 30, 40, 50};
    
    int sum = arr[0] + arr[1] + arr[2] + arr[3] + arr[4];
    // 10 + 20 + 30 + 40 + 50 = 150
    // 150 - 108 = 42
    return sum - 108;
}
