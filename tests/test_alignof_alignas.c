// Test _Alignof and _Alignas alignment control
// Tests compile-time alignment queries and alignment specifications

int main() {
    // Test 1: _Alignof with basic types
    if (_Alignof(char) != 1) return 1;
    if (_Alignof(short) != 2) return 2;
    if (_Alignof(int) < 4) return 3;
    if (_Alignof(long) < 8) return 4;
    if (_Alignof(double) < 8) return 5;
    
    // Test 2: _Alignof with pointer
    if (_Alignof(int*) < 8) return 6;
    if (_Alignof(void*) < 8) return 7;
    
    // Test 3: _Alignof with struct
    struct S1 {
        char c;
        int i;
    };
    if (_Alignof(struct S1) < 4) return 8;
    
    // Test 4: _Alignof with array
    int arr[10];
    if (_Alignof(arr) < 4) return 9;
    
    // Test 5: _Alignof with variable
    int x = 42;
    if (_Alignof(x) < 4) return 10;
    
    // Test 6: _Alignas with integer (2-byte alignment)
    _Alignas(2) char c1 = 'A';
    if (c1 != 'A') return 11;
    
    // Test 7: _Alignas with type
    _Alignas(int) char c2 = 'B';
    if (c2 != 'B') return 12;
    
    // Test 8: _Alignas with larger alignment
    _Alignas(16) int aligned_int = 100;
    if (aligned_int != 100) return 13;
    
    // Test 9: _Alignas in struct
    struct AlignedStruct {
        _Alignas(8) char c;
        int i;
    };
    struct AlignedStruct as = {'X', 200};
    if (as.c != 'X' || as.i != 200) return 14;
    
    // Test 10: Multiple aligned variables
    _Alignas(8) char a1 = '1';
    _Alignas(8) char a2 = '2';
    _Alignas(8) char a3 = '3';
    if (a1 != '1' || a2 != '2' || a3 != '3') return 15;
    
    return 42;  // Success
}
