// Debug test to see actual values

#pragma macro
long negate(long val) {
    return AST_UNARY(ND_NEG, (Node*)val);
}

#pragma macro
long multiply_and_add(long a, long b, long c) {
    Node *mul = AST_BINARY(ND_MUL, (Node*)a, (Node*)b);
    Node *add = AST_BINARY(ND_ADD, mul, (Node*)c);
    return (long)add;
}

// Forward declarations for printf
int printf(const char *, ...);

int main() {
    // Test: multiply_and_add(negate(2), 3, 4) should be (-2)*3 + 4 = -6 + 4 = -2
    int result = multiply_and_add(negate(2), 3, 4);
    printf("Result: %d (expected: -2)\n", result);

    // Also test the components separately
    int neg_result = negate(2);
    printf("negate(2) = %d (expected: -2)\n", neg_result);

    int mul_result = multiply_and_add(negate(2), 3, 0);
    printf("(-2) * 3 = %d (expected: -6)\n", mul_result);

    return 0;
}
