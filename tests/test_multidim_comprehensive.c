// Comprehensive multi-dimensional array test
// Expected return: 42

int main() {
    // Test 1: 2D array with nested loop initialization
    int matrix[3][4];
    int val = 1;
    for (int i = 0; i < 3; i = i + 1) {
        for (int j = 0; j < 4; j = j + 1) {
            matrix[i][j] = val;
            val = val + 1;
        }
    }
    
    // Verify: matrix[2][3] should be 12
    if (matrix[2][3] != 12) return 1;
    
    // Test 2: 2D array initialization with literals
    int grid[2][3] = {
        {10, 20, 30},
        {40, 50, 60}
    };
    
    int sum = 0;
    for (int i = 0; i < 2; i = i + 1) {
        for (int j = 0; j < 3; j = j + 1) {
            sum = sum + grid[i][j];
        }
    }
    // sum = 10+20+30+40+50+60 = 210
    if (sum != 210) return 2;
    
    // Test 3: 3D array access
    int cube[2][2][2];
    cube[0][0][0] = 1;
    cube[0][0][1] = 2;
    cube[0][1][0] = 3;
    cube[0][1][1] = 4;
    cube[1][0][0] = 5;
    cube[1][0][1] = 6;
    cube[1][1][0] = 7;
    cube[1][1][1] = 8;
    
    // Access diagonal
    int diag = cube[0][0][0] + cube[1][1][1];  // 1 + 8 = 9
    if (diag != 9) return 3;
    
    // Test 4: Pointer arithmetic with 2D arrays
    int arr[2][3] = {{1, 2, 3}, {4, 5, 6}};
    int *ptr = &arr[0][0];
    
    // Access via pointer arithmetic
    if (*(ptr + 0) != 1) return 4;  // arr[0][0]
    if (*(ptr + 2) != 3) return 5;  // arr[0][2]
    if (*(ptr + 3) != 4) return 6;  // arr[1][0]
    if (*(ptr + 5) != 6) return 7;  // arr[1][2]
    
    // Test 5: Array of arrays
    int row1[3] = {1, 2, 3};
    int row2[3] = {4, 5, 6};
    
    if (row1[1] + row2[1] != 7) return 8;  // 2 + 5 = 7
    
    // Test 6: Modifying through indices
    matrix[1][2] = 999;
    if (matrix[1][2] != 999) return 9;
    
    // Test 7: Character 2D arrays (strings)
    char strings[2][10];
    strings[0][0] = 'H';
    strings[0][1] = 'i';
    strings[0][2] = 0;
    strings[1][0] = 'B';
    strings[1][1] = 'y';
    strings[1][2] = 'e';
    strings[1][3] = 0;
    
    if (strings[0][0] != 'H') return 10;
    if (strings[1][2] != 'e') return 11;
    
    // All tests passed!
    return 42;
}
