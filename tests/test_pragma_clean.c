// Test clean pragma macro API - no vm parameter needed!
// Demonstrates the __VM builtin (works like stdin/stdout/stderr)

#pragma macro
long make_constant(long value_node) {
    // No vm parameter! __VM is automatically available
    // value_node is a Node* - we just return it
    return value_node;
}

#pragma macro
long add_two_numbers(long a_node, long b_node) {
    // Arguments are Node pointers - create an AST node for: a + b
    return AST_BINARY(ND_ADD, (Node*)a_node, (Node*)b_node);
}

int main() {
    // Test 1: Simple constant generation
    int x = make_constant(42);
    if (x != 42) {
        return 1;
    }

    // Test 2: Binary operation
    int y = add_two_numbers(10, 20);
    if (y != 30) {
        return 2;
    }

    // Test 3: Nested calls
    int z = add_two_numbers(make_constant(5), make_constant(7));
    if (z != 12) {
        return 3;
    }

    return 0;  // All tests passed!
}
