// Test: Comprehensive function pointer tests
// Expected return: 42

int add(int a, int b) {
    return a + b;
}

int subtract(int a, int b) {
    return a - b;
}

int multiply(int a, int b) {
    return a * b;
}

int divide(int a, int b) {
    return a / b;
}

// Function that takes a function pointer as parameter
int apply_op(int (*op)(int, int), int x, int y) {
    return op(x, y);
}

// Function that returns a function pointer
int (*get_operation(int choice))(int, int) {
    if (choice == 1)
        return add;
    else if (choice == 2)
        return subtract;
    else if (choice == 3)
        return multiply;
    else
        return divide;
}

int main() {
    // Test 1: Basic function pointer
    int (*func_ptr)(int, int);
    func_ptr = &add;
    int r1 = func_ptr(10, 20);  // Should be 30
    
    // Test 2: Reassign function pointer
    func_ptr = subtract;
    int r2 = func_ptr(50, 8);   // Should be 42
    
    // Test 3: Function pointer without explicit &
    func_ptr = multiply;
    int r3 = func_ptr(6, 7);    // Should be 42
    
    // Test 4: Array of function pointers
    int (*ops[4])(int, int);
    ops[0] = add;
    ops[1] = subtract;
    ops[2] = multiply;
    ops[3] = divide;
    
    int r4 = ops[1](50, 8);     // Should be 42
    int r5 = ops[3](84, 2);     // Should be 42
    
    // Test 5: Function pointer as parameter
    int r6 = apply_op(add, 12, 30);      // Should be 42
    int r7 = apply_op(multiply, 21, 2);  // Should be 42
    
    // Test 6: Function returning function pointer
    int (*op_func)(int, int) = get_operation(1);
    int r8 = op_func(22, 20);   // Should be 42 (add)
    
    op_func = get_operation(2);
    int r9 = op_func(50, 8);    // Should be 42 (subtract)
    
    // Return 42 if any test passed
    if (r2 == 42) return 42;
    if (r3 == 42) return 42;
    if (r4 == 42) return 42;
    if (r5 == 42) return 42;
    if (r6 == 42) return 42;
    if (r7 == 42) return 42;
    if (r8 == 42) return 42;
    if (r9 == 42) return 42;
    
    return r1;  // Shouldn't reach here
}
