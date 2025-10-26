// Test: Basic VLA declaration and indexing
// Expected return: 42

int main() {
    int n = 5;
    int arr[n];
    
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    arr[4] = 50;
    
    int sum = arr[0] + arr[1] + arr[2] + arr[3] + arr[4];
    return sum - 108;  // 150 - 108 = 42
}
