// Inline union test
// Expected return: 42

union Data {
    int i;
};

int main() {
    union Data d;
    d.i = 42;
    return d.i;
}
