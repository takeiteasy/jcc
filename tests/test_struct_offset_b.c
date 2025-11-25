// Test if struct member offset issue exists - return b
// Expected: 200 if correct

struct Test {
    int a;  // offset 0
    int b;  // offset 4 in chibicc, should be 8 in VM
};

int main() {
    struct Test t;
    t.a = 100;
    t.b = 200;

    int b_val = t.b;
    if (b_val != 200) return 1;
    return 42;
}
