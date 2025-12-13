// Test volatile keyword parsing
// Both volatile int and volatile pointer should be accepted

volatile int global_volatile = 42;

int main() {
    // Test volatile local variable
    volatile int x = 10;
    
    // Test volatile pointer
    volatile int *vp = &x;
    
    // Test pointer to volatile
    int * volatile pv;
    
    // Test const volatile
    const volatile int cv = 100;
    
    // Read volatile variable
    int a = x;
    
    // Write volatile variable
    x = 20;
    
    // Read through volatile pointer
    int b = *vp;
    
    // Verify values are as expected
    if (a != 10) return 1;
    if (b != 20) return 2;
    if (cv != 100) return 3;
    if (global_volatile != 42) return 4;
    
    return 42;
}
