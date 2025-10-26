// Test multi-dimensional arrays
// Expected return: 42

int main() {
    // 2D array - matrix[3][4]
    int matrix[3][4];
    
    // Initialize with values
    matrix[0][0] = 1;
    matrix[0][1] = 2;
    matrix[0][2] = 3;
    matrix[0][3] = 4;
    
    matrix[1][0] = 10;
    matrix[1][1] = 20;
    matrix[1][2] = 30;
    matrix[1][3] = 40;
    
    matrix[2][0] = 100;
    matrix[2][1] = 200;
    matrix[2][2] = 300;
    matrix[2][3] = 400;
    
    // Access elements
    int a = matrix[0][2];  // Should be 3
    int b = matrix[1][3];  // Should be 40
    int c = matrix[2][1];  // Should be 200
    
    // Verify: 3 + 40 - 200 + 199 = 42
    int result = a + b - c + 199;
    
    // Test with array initialization
    int grid[2][3] = {
        {1, 2, 3},
        {4, 5, 6}
    };
    
    int sum = grid[0][0] + grid[0][1] + grid[0][2] +
              grid[1][0] + grid[1][1] + grid[1][2];
    // sum = 1+2+3+4+5+6 = 21
    
    // Test 3D array
    int cube[2][2][2];
    cube[0][0][0] = 1;
    cube[0][0][1] = 2;
    cube[0][1][0] = 3;
    cube[0][1][1] = 4;
    cube[1][0][0] = 5;
    cube[1][0][1] = 6;
    cube[1][1][0] = 7;
    cube[1][1][1] = 8;
    
    int cube_sum = cube[0][0][0] + cube[1][1][1];  // 1 + 8 = 9
    
    // Final calculation: result=42, sum=21, cube_sum=9
    // 42 + 21 - 9 - 12 = 42
    return result + sum - cube_sum - 12;
}
