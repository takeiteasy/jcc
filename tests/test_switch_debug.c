// Minimal switch test with explicit values
int main() {
    int x = 10;
    switch (x) {
        case 0: return 0;
        case 1: return 1;
        case 10: return 42;  // Should match
        case 11: return 11;
        default: return 99;
    }
}
