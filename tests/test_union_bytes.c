// Test union byte-level access pattern
// Expected return: 42

union ByteAccess {
    int value;
    struct {
        char byte0;
        char byte1;
        char byte2;
        char byte3;
    } bytes;
};

int main() {
    union ByteAccess u;
    
    // Write as integer
    u.value = 0x2A1E0F05;  // 42, 30, 15, 5 in little-endian bytes
    
    // Read individual bytes (little-endian: LSB first)
    int b0 = u.bytes.byte0;  // Should be 0x05 (5)
    int b1 = u.bytes.byte1;  // Should be 0x0F (15)
    int b2 = u.bytes.byte2;  // Should be 0x1E (30)
    int b3 = u.bytes.byte3;  // Should be 0x2A (42)
    
    // Reconstruct value from bytes
    // Just verify we can read the top byte
    return b3;  // Should be 42
}
