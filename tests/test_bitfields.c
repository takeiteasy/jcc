// Comprehensive bitfield tests

struct bits1 {
    unsigned int a : 3;
    unsigned int b : 5;
    unsigned int c : 8;
};

struct bits2 {
    int x : 4;   // Signed bitfield
    int y : 4;
};

struct mixed {
    unsigned int flags : 1;
    int value;
    unsigned int more : 7;
};

int main() {
    // Test 1: Basic unsigned bitfield write/read
    struct bits1 x = {0};
    x.a = 7;   // 3 bits: max value
    x.b = 31;  // 5 bits: max value
    x.c = 255; // 8 bits: max value

    if (x.a != 7)
        return 1;
    if (x.b != 31)
        return 2;
    if (x.c != 255)
        return 3;

    // Test 2: Overflow wrapping
    x.a = 8;  // Should wrap to 0 (3 bits)
    if (x.a != 0)
        return 4;

    x.a = 15;  // Should wrap to 7 (3 bits)
    if (x.a != 7)
        return 5;

    // Test 3: Signed bitfields
    struct bits2 y = {0};
    y.x = 7;   // Max positive for 4-bit signed (-8 to 7)
    y.y = -8;  // Min negative for 4-bit signed

    if (y.x != 7)
        return 6;
    if (y.y != -8)
        return 7;

    // Test 4: Mixed struct with bitfields and regular members
    struct mixed m = {0};
    m.flags = 1;
    m.value = 12345;
    m.more = 127;

    if (m.flags != 1)
        return 8;
    if (m.value != 12345)
        return 9;
    if (m.more != 127)
        return 10;

    // Test 5: Bitfield assignment doesn't affect adjacent fields
    struct bits1 z = {0};
    z.b = 31;
    if (z.a != 0 || z.c != 0)
        return 11;

    z.a = 5;
    if (z.b != 31)  // b should still be 31
        return 12;

    return 42;
}
