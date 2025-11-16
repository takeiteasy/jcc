/*
 * Atomic Compound Assignment Test
 * Tests _Atomic type qualifier with compound assignments
 *
 * The JCC parser automatically transforms compound assignments on _Atomic
 * variables into CAS retry loops.
 */

int main() {
    _Atomic int counter = 0;

    // Test 1: Atomic addition
    counter += 1;
    if (counter != 1) return 1;

    counter += 10;
    if (counter != 11) return 2;

    // Test 2: Atomic subtraction
    counter -= 5;
    if (counter != 6) return 3;

    // Test 3: Atomic multiplication
    counter *= 2;
    if (counter != 12) return 4;

    // Test 4: Atomic division
    counter /= 3;
    if (counter != 4) return 5;

    // Test 5: Atomic modulo
    counter %= 3;
    if (counter != 1) return 6;

    // Test 6: Bitwise operations
    _Atomic int bits = 0xFF;

    bits &= 0x0F;
    if (bits != 0x0F) return 7;

    bits |= 0xF0;
    if (bits != 0xFF) return 8;

    bits ^= 0xAA;
    if (bits != 0x55) return 9;

    // Test 7: Shift operations
    _Atomic int shift = 1;

    shift <<= 3;
    if (shift != 8) return 10;

    shift >>= 2;
    if (shift != 2) return 11;

    // Test 8: Multiple atomic variables
    _Atomic long a = 100;
    _Atomic long b = 200;

    a += 50;
    b -= 50;

    if (a != 150) return 12;
    if (b != 150) return 13;

    // Test 9: Atomic increment/decrement
    _Atomic int inc = 5;
    inc++;
    if (inc != 6) return 14;

    inc--;
    if (inc != 5) return 15;

    return 0;  // All tests passed
}
