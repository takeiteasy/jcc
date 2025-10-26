// Test flexible array with long values
#include "stdlib.h"

struct buffer {
    int count;
    long values[];
};

int main() {
    // Test sizeof first
    int s = sizeof(struct buffer);
    if (s != 4) return 10 + s;  // Return 10 + actual size if wrong
    
    // Allocate
    struct buffer *buf = malloc(sizeof(struct buffer) + 3 * sizeof(long));
    if (!buf) return 20;  // malloc failed
    
    // Set values
    buf->count = 3;
    buf->values[0] = 100;
    buf->values[1] = 200;
    buf->values[2] = 300;
    
    // Check values
    if (buf->count != 3) return 30;
    if (buf->values[0] != 100) return 40;
    if (buf->values[1] != 200) return 50;
    if (buf->values[2] != 300) return 60;
    
    free(buf);
    return 0;  // SUCCESS
}
