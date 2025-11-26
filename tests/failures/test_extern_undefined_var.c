// Test for undefined extern variable - should fail at link time
extern int undefined_var;

int main() {
    // Try to use undefined variable
    return undefined_var;
}
