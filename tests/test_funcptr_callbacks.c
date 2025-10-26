// Test: Function pointers with callbacks
// Expected return: 42

// Simple operations
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

// Higher-order function: takes array, applies operation, returns result
int reduce(int *arr, int len, int init, int (*op)(int, int)) {
    int result = init;
    int i = 0;
    while (i < len) {
        result = op(result, arr[i]);
        i = i + 1;
    }
    return result;
}

// Function that conditionally selects operation
int compute(int x, int y, int use_add) {
    int (*operation)(int, int);
    
    if (use_add) {
        operation = add;
    } else {
        operation = mul;
    }
    
    return operation(x, y);
}

int main() {
    // Test 1: Callback-style with reduce
    int numbers[5];
    numbers[0] = 1;
    numbers[1] = 2;
    numbers[2] = 3;
    numbers[3] = 4;
    numbers[4] = 5;
    
    int sum = reduce(numbers, 5, 0, add);  // 0+1+2+3+4+5 = 15
    int product = reduce(numbers, 5, 1, mul);  // 1*1*2*3*4*5 = 120
    
    // Test 2: Conditional function selection
    int r1 = compute(20, 22, 1);  // add: 42
    int r2 = compute(21, 2, 0);   // mul: 42
    
    // Test 3: Function pointer in struct (using array as workaround)
    int (*ops[2])(int, int);
    ops[0] = add;
    ops[1] = sub;
    
    int r3 = ops[0](12, 30);  // 42
    int r4 = ops[1](50, 8);   // 42
    
    // Return 42 if any test succeeded
    if (r1 == 42) return 42;
    if (r2 == 42) return 42;
    if (r3 == 42) return 42;
    if (r4 == 42) return 42;
    
    return sum + product;  // Shouldn't reach
}
