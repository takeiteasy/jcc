// Simple test - what value do we get?
int main() {
    int i = 1000;
    char c = (char)i;
    // 1000 in binary: 0x3E8
    // As byte: 0xE8 = 232 unsigned, -24 signed
    return (c == -24) ? 0 : 1;
}
