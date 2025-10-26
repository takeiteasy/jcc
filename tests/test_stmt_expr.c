// Test statement expressions (GNU extension)
// Expected return: 42

int main() {
    // Test 1: Basic statement expression
    int x = ({
        int a = 10;
        int b = 32;
        a + b;  // Last expression is the value
    });
    if (x != 42) return 1;
    
    // Test 2: Statement expression with control flow
    int max = ({
        int a = 5;
        int b = 7;
        int result = a;
        if (b > a) result = b;
        result;
    });
    if (max != 7) return 2;
    
    // Test 3: Nested statement expressions
    int y = ({
        int inner = ({
            int z = 20;
            z + 2;
        });
        inner + 20;
    });
    if (y != 42) return 3;
    
    return 42;  // Success
}
