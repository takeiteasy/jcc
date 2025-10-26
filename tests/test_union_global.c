// Test global union
// Expected return: 42

union Global {
    int val;
    char ch;
} global_union;

int main() {
    global_union.val = 42;
    return global_union.val;
}
