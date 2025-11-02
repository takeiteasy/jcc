// Test more complex pragma macro operations

#pragma macro
long multiply_and_add(long a, long b, long c) {
    // Create (a * b) + c
    Node *mul = AST_BINARY(ND_MUL, (Node*)a, (Node*)b);
    Node *add = AST_BINARY(ND_ADD, mul, (Node*)c);
    return (long)add;
}

#pragma macro
long negate(long val) {
    // Create -val
    return AST_UNARY(ND_NEG, (Node*)val);
}

#pragma macro
long triple_add(long a, long b, long c) {
    // Create a + b + c
    Node *sum1 = AST_BINARY(ND_ADD, (Node*)a, (Node*)b);
    Node *sum2 = AST_BINARY(ND_ADD, sum1, (Node*)c);
    return (long)sum2;
}

int main() {
    // Test 1: multiply_and_add(3, 4, 5) = 3*4 + 5 = 17
    int result1 = multiply_and_add(3, 4, 5);
    if (result1 != 17) {
        return 1;
    }

    // Test 2: negate(10) = -10
    int result2 = negate(10);
    if (result2 != -10) {
        return 2;
    }

    // Test 3: triple_add(1, 2, 3) = 6
    int result3 = triple_add(1, 2, 3);
    if (result3 != 6) {
        return 3;
    }

    // Test 4: nested - multiply_and_add(negate(2), 3, 4) = -2*3 + 4 = -2
    int result4 = multiply_and_add(negate(2), 3, 4);
    if (result4 != -2) {
        return 4;
    }

    return 0;  // All tests passed
}
