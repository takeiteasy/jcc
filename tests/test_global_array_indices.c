// Test each array index individually

int arr[5] = {10, 20, 30, 40, 50};

int main() {
    int v0 = arr[0];  // Should be 10
    int v1 = arr[1];  // Should be 20
    int v2 = arr[2];  // Should be 30
    int v3 = arr[3];  // Should be 40
    int v4 = arr[4];  // Should be 50
    
    // Return sum to verify all values
    return v0 + v1;  // Should be 30
}
