// Simple test for flexible array members
#include "stdlib.h"

struct packet {
    int size;
    char data[];
};

int main() {
    // Test 1: Check sizeof
    int s = sizeof(struct packet);
    // In VM, int is 4 bytes, so sizeof should be 4
    if (s != 4) return 1;  // FAIL
    
    // Test 2: Allocate and use
    struct packet *p = malloc(sizeof(struct packet) + 10);
    if (!p) return 2;  // FAIL - malloc failed
    
    p->size = 10;
    p->data[0] = 65;  // 'A'
    p->data[1] = 66;  // 'B'
    
    if (p->size != 10) return 3;  // FAIL
    if (p->data[0] != 65) return 4;  // FAIL
    if (p->data[1] != 66) return 5;  // FAIL
    
    free(p);
    return 42;  // SUCCESS - all tests passed
}
