// Test array compound literals with pointer assignment
// Expected return: 42

int main() {
    // Test 1: Array compound literal assigned to pointer
    int *p1 = (int[]){10, 20, 12};
    if (p1[0] + p1[1] + p1[2] != 42) return 1;
    
    // Test 2: Array compound literal with direct access
    int val = ((int[]){42, 0})[0];
    if (val != 42) return 2;
    
    // Test 3: Multi-dimensional array compound literal
    int *p2 = (int[]){1, 2, 3, 4, 5};
    int sum = p2[0] + p2[1] + p2[2] + p2[3] + p2[4];
    if (sum != 15) return 3;
    
    return 42;
}
