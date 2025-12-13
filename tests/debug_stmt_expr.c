// Debug stmt expr - why is a+b returning 64 instead of 42?
int main() {
    int t6 = ({ int a = 10; int b = 32; a + b; });
    // Expected: 42, got: 64
    // 64 = 32 * 2, so it looks like b + b
    return t6;
}
