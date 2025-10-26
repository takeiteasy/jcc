// Test _Static_assert compile-time assertions

// Global scope assertions
_Static_assert(1, "This should pass");
_Static_assert(sizeof(int) >= 4, "int must be at least 4 bytes");
_Static_assert(sizeof(char) == 1, "char must be 1 byte");

int main() {
    // Function scope assertions
    _Static_assert(1 + 1 == 2, "Math doesn't work!");
    _Static_assert(10 > 5, "Comparison doesn't work!");
    
    // Test with sizeof
    _Static_assert(sizeof(long) >= 8, "long must be at least 8 bytes");
    _Static_assert(sizeof(double) == 8, "double must be 8 bytes");
    
    // Test with expressions
    _Static_assert((3 * 4) == 12, "multiplication doesn't work");
    _Static_assert((20 / 4) == 5, "division doesn't work");
    
    // Test with bitwise operations
    _Static_assert((0xFF & 0x0F) == 0x0F, "bitwise AND doesn't work");
    _Static_assert((0xF0 | 0x0F) == 0xFF, "bitwise OR doesn't work");
    
    // Test in nested blocks
    {
        _Static_assert(1, "nested block assertion");
        {
            _Static_assert(1, "deeply nested assertion");
        }
    }
    
    // Test with pointer sizes
    _Static_assert(sizeof(void*) == 8, "pointers must be 8 bytes");
    _Static_assert(sizeof(int*) == 8, "int pointers must be 8 bytes");
    
    return 42;  // Success
}

// Note: To test failure cases, uncomment these (they should cause compile errors):
// _Static_assert(0, "This should fail at compile time");
// _Static_assert(1 == 2, "This should also fail");
// _Static_assert(sizeof(int) < 4, "This will fail on most platforms");
