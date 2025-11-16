// Very simple atomic test
int main() {
    int x = 42;
    int old = __jcc_atomic_exchange(&x, 100);
    return 0;
}
