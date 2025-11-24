// Test labels-as-values and computed goto (GNU extension)

int main() {
    void *target;
    void *jump_table[] = {&&a, &&b, &&c};  // Array initializer with labels
    int index;
    int x;
    void *next;

    // Test 1: Basic label-as-value and computed goto
    target = &&label1;
    goto *target;

    return 1;  // Should not reach here

label1:
    // Test 2: Jump table using label addresses (with initializer)
    index = 1;
    goto *jump_table[index];

a:
    return 10;

b:
    // Test 3: Conditional computed goto
    x = 5;
    next = (x > 3) ? &&success : &&fail;
    goto *next;

c:
    return 30;

fail:
    return 99;

success:
    return 42;
}
