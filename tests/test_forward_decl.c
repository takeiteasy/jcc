// Test forward declarations (incomplete types)
// Expected return: 42

// Forward declarations
struct Node;
struct Data;
union Value;

// Function that uses forward-declared pointer types
struct Node *create_node(int val);
int get_node_value(struct Node *n);

// Complete the struct definitions
struct Node {
    int value;
    struct Node *next;  // Self-referential using forward declaration
};

struct Data {
    int x;
    int y;
    struct Node *node_ptr;  // Pointer to previously forward-declared type
};

union Value {
    int i;
    char c;
};

// Function implementations
struct Node *create_node(int val) {
    // Note: We can't actually allocate memory without malloc,
    // so we'll use a static variable as a workaround
    static struct Node n;
    n.value = val;
    n.next = 0;  // NULL
    return &n;
}

int get_node_value(struct Node *n) {
    return n->value;
}

int main() {
    // Test 1: Create node using forward-declared function
    struct Node *node = create_node(20);
    if (get_node_value(node) != 20) return 1;
    
    // Test 2: Use struct with pointer to forward-declared type
    struct Data d;
    d.x = 10;
    d.y = 12;
    d.node_ptr = node;
    
    // Test 3: Access through pointer
    int sum = d.x + d.y + d.node_ptr->value;  // 10 + 12 + 20 = 42
    if (sum != 42) return 2;
    
    // Test 4: Self-referential struct (linked list node)
    struct Node n1;
    n1.value = 15;
    n1.next = 0;
    
    struct Node n2;
    n2.value = 27;
    n2.next = &n1;
    
    // Traverse: n2->value + n2->next->value = 27 + 15 = 42
    int result = n2.value + n2.next->value;
    if (result != 42) return 3;
    
    // Test 5: Forward-declared union
    union Value v;
    v.i = 42;
    if (v.i != 42) return 4;
    
    return 42;
}
