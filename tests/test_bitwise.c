int main() {
    int a = 12;  // 1100 in binary
    int b = 10;  // 1010 in binary
    
    // Bitwise operations
    int or_result = a | b;      // 1110 = 14
    int xor_result = a ^ b;     // 0110 = 6
    int and_result = a & b;     // 1000 = 8
    int shl_result = 1 << 3;    // 8
    int shr_result = 16 >> 2;   // 4
    
    // Sum: 14 + 6 + 8 + 8 + 4 = 40
    int result = or_result + xor_result + and_result + shl_result + shr_result;
    
    return result;
}
