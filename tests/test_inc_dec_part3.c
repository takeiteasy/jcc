// Test part 3 - pointers
int main() {
    // Test combined operations
    int xx = 5;
    int yy = 10;
    int result5 = xx++ + ++yy;
    if (xx != 6) return 1;
    if (yy != 11) return 2;
    if (result5 != 16) return 3;
    
    // Test increment dereferenced pointer
    int z = 42;
    int *ptr = &z;
    (*ptr)++;
    if (z != 43) return 4;
    if (*ptr != 43) return 5;
    
    // Test pre-increment with pointer dereference
    int w = 100;
    int *ptr2 = &w;
    int result6 = ++(*ptr2);
    if (w != 101) return 6;
    if (result6 != 101) return 7;
    
    // Test post-increment with pointer dereference
    int v = 200;
    int *ptr3 = &v;
    int result7 = (*ptr3)++;
    if (v != 201) return 8;
    if (result7 != 200) return 9;
    
    return 42;  // All passed
}
