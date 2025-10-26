// Test increment and decrement operators (++ and --)
// Tests both pre-increment/decrement and post-increment/decrement

int main() {
    // Test 1: Post-increment (i++)
    int a = 5;
    int b = a++;  // b = 5, a = 6
    if (b != 5) return 1;
    if (a != 6) return 2;
    
    // Test 2: Pre-increment (++i)
    int c = 5;
    int d = ++c;  // d = 6, c = 6
    if (d != 6) return 3;
    if (c != 6) return 4;
    
    // Test 3: Post-decrement (i--)
    int e = 10;
    int f = e--;  // f = 10, e = 9
    if (f != 10) return 5;
    if (e != 9) return 6;
    
    // Test 4: Pre-decrement (--i)
    int g = 10;
    int h = --g;  // h = 9, g = 9
    if (h != 9) return 7;
    if (g != 9) return 8;
    
    // Test 5: Multiple increments
    int i = 0;
    i++;
    i++;
    i++;
    if (i != 3) return 9;
    
    // Test 6: Multiple decrements
    int j = 5;
    j--;
    j--;
    if (j != 3) return 10;
    
    // Test 7: Pre-increment in expression
    int k = 5;
    int result1 = ++k + 10;  // k = 6, result1 = 16
    if (k != 6) return 11;
    if (result1 != 16) return 12;
    
    // Test 8: Post-increment in expression
    int m = 5;
    int result2 = m++ + 10;  // m = 6, result2 = 15
    if (m != 6) return 13;
    if (result2 != 15) return 14;
    
    // Test 9: Pre-decrement in expression
    int n = 10;
    int result3 = --n * 2;  // n = 9, result3 = 18
    if (n != 9) return 15;
    if (result3 != 18) return 16;
    
    // Test 10: Post-decrement in expression
    int p = 10;
    int result4 = p-- * 2;  // p = 9, result4 = 20
    if (p != 9) return 17;
    if (result4 != 20) return 18;
    
    // Test 11: Increment in loop condition
    int count = 0;
    for (int x = 0; x < 5; x++) {
        count++;
    }
    if (count != 5) return 19;
    
    // Test 12: Decrement in loop condition
    int count2 = 0;
    for (int y = 10; y > 5; y--) {
        count2++;
    }
    if (count2 != 5) return 20;
    
    // Test 13: Combined operations
    int xx = 5;
    int yy = 10;
    int result5 = xx++ + ++yy;  // xx = 6, yy = 11, result5 = 5 + 11 = 16
    if (xx != 6) return 21;
    if (yy != 11) return 22;
    if (result5 != 16) return 23;
    
    // Test 14: Increment dereferenced pointer
    int z = 42;
    int *ptr = &z;
    (*ptr)++;  // Increment the value pointed to
    if (z != 43) return 24;
    if (*ptr != 43) return 25;
    
    // Test 15: Pre-increment with pointer dereference
    int w = 100;
    int *ptr2 = &w;
    int result6 = ++(*ptr2);  // w = 101, result6 = 101
    if (w != 101) return 26;
    if (result6 != 101) return 27;
    
    // Test 16: Post-increment with pointer dereference
    int v = 200;
    int *ptr3 = &v;
    int result7 = (*ptr3)++;  // v = 201, result7 = 200
    if (v != 201) return 28;
    if (result7 != 200) return 29;
    
    // Test 17: Multiple operations in sequence
    int seq = 10;
    seq++;  // 11
    seq++;  // 12
    seq--;  // 11
    seq--;  // 10
    if (seq != 10) return 30;
    
    // Test 18: Increment in conditional
    int cond = 5;
    if (++cond == 6) {
        if (cond != 6) return 31;
    } else {
        return 32;
    }
    
    // All tests passed!
    return 42;
}
