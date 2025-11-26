// Test for undefined extern symbols - should fail at link time
extern int undefined_var;
extern int undefined_func(int x);

int main() {
    // Try to use undefined symbols
    int result = undefined_var + undefined_func(10);
    return result;
}
