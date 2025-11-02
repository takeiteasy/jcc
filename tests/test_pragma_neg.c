// Test negate pragma macro alone

#pragma macro
long negate(long val) {
    return AST_UNARY(ND_NEG, (Node*)val);
}

int main() {
    int result = negate(2);
    // Should be -2
    return result == -2 ? 0 : 1;
}
