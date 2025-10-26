// Test: Ultra minimal - just create VLA, don't use it
// Expected return: 1

int main() {
    int n = 1;
    int arr[n];
    return n;  // Don't even touch arr
}
