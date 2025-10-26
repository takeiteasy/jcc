// Test memory leak detection with VLAs
// VLAs use the VM's MALC/MFRE instructions

int main() {
    int n = 10;

    // This VLA allocation uses MALC internally and will leak
    {
        int arr1[n];  // VLA - uses MALC
        arr1[0] = 42;
    }
    // arr1 should be freed here, but let's see if leak detection works

    return 0;
}
