// Comprehensive enum test
enum Operation {
    OP_ADD = 1,
    OP_SUB = 2,
    OP_MUL = 3,
    OP_DIV = 4
};

enum Priority {
    LOW,
    MEDIUM = 5,
    HIGH = 10
};

int calculate(enum Operation op, int a, int b) {
    switch (op) {
        case OP_ADD: return a + b;
        case OP_SUB: return a - b;
        case OP_MUL: return a * b;
        case OP_DIV: return a / b;
        default: return 0;
    }
}

int main() {
    // Test basic enum usage
    enum Operation op = OP_MUL;
    enum Priority p = HIGH;
    
    // Test in expressions
    int result = calculate(op, 6, 7);  // 6 * 7 = 42
    
    // Test enum comparison and arithmetic
    if (p == HIGH) {
        result = result + LOW;  // 42 + 0 = 42
    }
    
    // Test enum in array subscript
    int values[4] = {0, 10, 20, 30};
    int x = values[OP_SUB];  // values[2] = 20
    
    // Test bitwise operations with enums
    enum Priority combined = LOW | MEDIUM;  // 0 | 5 = 5
    
    return result;  // Should return 42
}
