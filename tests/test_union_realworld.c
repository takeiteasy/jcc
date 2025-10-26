// Real-world union example: Type-tagged value system
// Expected return: 42

// Type tags
#define TYPE_INT  1
#define TYPE_CHAR 2
#define TYPE_PTR  3

// Tagged union for generic value storage
struct Value {
    int type;
    union {
        int i;
        char c;
        void *ptr;
    } data;
};

int main() {
    int score = 0;
    
    // Test 1: Integer value
    struct Value v1;
    v1.type = TYPE_INT;
    v1.data.i = 100;
    
    if (v1.type == TYPE_INT && v1.data.i == 100) {
        score += 10;
    }
    
    // Test 2: Char value (overwrites the same memory)
    struct Value v2;
    v2.type = TYPE_CHAR;
    v2.data.c = 'A';
    
    if (v2.type == TYPE_CHAR && v2.data.c == 'A') {
        score += 10;
    }
    
    // Test 3: Type switching on same variable
    struct Value v3;
    v3.type = TYPE_INT;
    v3.data.i = 200;
    
    // Switch type to char
    v3.type = TYPE_CHAR;
    v3.data.c = 42;
    
    if (v3.type == TYPE_CHAR && v3.data.c == 42) {
        score += 10;
    }
    
    // Test 4: Array of tagged values
    struct Value arr[3];
    
    arr[0].type = TYPE_INT;
    arr[0].data.i = 1;
    
    arr[1].type = TYPE_INT;
    arr[1].data.i = 2;
    
    arr[2].type = TYPE_INT;
    arr[2].data.i = 3;
    
    int sum = arr[0].data.i + arr[1].data.i + arr[2].data.i;
    if (sum == 6) {
        score += 12;
    }
    
    return score;  // Should be 42
}
