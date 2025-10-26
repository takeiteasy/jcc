// Simple VLA cleanup test
// Expected return: 42

int main() {
    int n = 5;
    int arr[n];
    
    arr[0] = 42;
    int result = arr[0];
    
    return result;
}
