// Test: VLA in nested scope
// Expected return: 30

int main() {
    int result = 0;
    
    {
        int n = 3;
        int arr1[n];
        arr1[0] = 10;
        arr1[1] = 20;
        arr1[2] = 30;
        result = arr1[2];
    }
    
    {
        int m = 2;
        int arr2[m];
        arr2[0] = 100;
        arr2[1] = 200;
        // result should still be 30 from above
    }

    if (result != 30) return 1;  // Assert result == 30
    return 42;
}
