// Test flexible array members (C99 feature)
#include "stdlib.h"

// Basic flexible array member
struct packet {
    int size;
    char data[];  // Flexible array member
};

// Test sizeof - should not include the flexible array
int test_sizeof() {
    // sizeof(struct packet) should be sizeof(int) only
    // In the VM, int is 4 bytes
    return sizeof(struct packet) == 4 ? 1 : 0;
}

// Test allocation and usage
int test_allocation() {
    // Allocate space for struct + 10 chars
    struct packet *p = malloc(sizeof(struct packet) + 10);
    if (!p) return 0;
    
    // Initialize
    p->size = 10;
    p->data[0] = 'H';
    p->data[1] = 'e';
    p->data[2] = 'l';
    p->data[3] = 'l';
    p->data[4] = 'o';
    p->data[5] = 0;
    
    // Verify
    int result = (p->size == 10 && 
                  p->data[0] == 'H' && 
                  p->data[4] == 'o' &&
                  p->data[5] == 0) ? 1 : 0;
    
    free(p);
    return result;
}

// Struct with multiple members before flexible array
struct message {
    int type;
    int length;
    char payload[];
};

int test_multiple_members() {
    // sizeof should be 2 * sizeof(int) = 8 in VM
    if (sizeof(struct message) != 8) return 0;
    
    struct message *msg = malloc(sizeof(struct message) + 5);
    if (!msg) return 0;
    
    msg->type = 42;
    msg->length = 5;
    msg->payload[0] = 'T';
    msg->payload[1] = 'e';
    msg->payload[2] = 's';
    msg->payload[3] = 't';
    msg->payload[4] = 0;
    
    int result = (msg->type == 42 && 
                  msg->length == 5 &&
                  msg->payload[0] == 'T' &&
                  msg->payload[3] == 't') ? 1 : 0;
    
    free(msg);
    return result;
}

// Test with different types
struct buffer {
    int count;
    long values[];
};

int test_different_types() {
    // sizeof should be 8 (int=4 + 4 bytes padding for long alignment)
    if (sizeof(struct buffer) != 8) return 0;
    
    struct buffer *buf = malloc(sizeof(struct buffer) + 3 * sizeof(long));
    if (!buf) return 0;
    
    buf->count = 3;
    buf->values[0] = 100;
    buf->values[1] = 200;
    buf->values[2] = 300;
    
    int result = (buf->count == 3 &&
                  buf->values[0] == 100 &&
                  buf->values[1] == 200 &&
                  buf->values[2] == 300) ? 1 : 0;
    
    free(buf);
    return result;
}

int main() {
    int passed = 0;
    
    // Test 1: sizeof
    int t1 = test_sizeof();
    if (t1) {
        passed++;
    }
    
    // Test 2: allocation
    int t2 = test_allocation();
    if (t2) {
        passed++;
    }
    
    // Test 3: multiple members
    int t3 = test_multiple_members();
    if (t3) {
        passed++;
    }
    
    // Test 4: different types
    int t4 = test_different_types();
    if (t4) {
        passed++;
    }
    
    // Return number of tests passed (out of 4)
    // Expected: 4 (all pass)
    if (passed != 4) return 1;  // Assert all 4 tests passed
    return 42;
}
