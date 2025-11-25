// Test negative value directly  
int main() {
    int a = -5;
    a++;
    // If a is -4, then -a is 4
    int result = 0 - a;
    if (result != 4) return 1;
    return 42;
}
