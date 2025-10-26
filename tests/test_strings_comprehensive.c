// Comprehensive test for string literals

int main() {
    // Test 1: Basic string literal assignment
    char *str1 = "Hello";
    if (str1[0] != 'H') return 1;
    if (str1[1] != 'e') return 2;
    if (str1[2] != 'l') return 3;
    if (str1[3] != 'l') return 4;
    if (str1[4] != 'o') return 5;
    if (str1[5] != '\0') return 6;
    
    // Test 2: Multiple string literals
    char *str2 = "World";
    if (str2[0] != 'W') return 7;
    if (str2[4] != 'd') return 8;
    
    // Test 3: Empty string
    char *empty = "";
    if (empty[0] != '\0') return 9;
    
    // Test 4: String with special characters
    char *special = "A\nB\tC";
    if (special[0] != 'A') return 10;
    if (special[1] != '\n') return 11;
    if (special[2] != 'B') return 12;
    if (special[3] != '\t') return 13;
    if (special[4] != 'C') return 14;
    
    // Test 5: String pointers can be compared
    char *dup1 = "Test";
    char *dup2 = "Test";
    // Note: In this implementation, same string literals 
    // may not be deduplicated, so we just test access
    if (dup1[0] != 'T') return 15;
    if (dup2[0] != 'T') return 16;
    
    // Test 6: Pointer arithmetic with strings
    char *ptr = "ABCDEF";
    char *ptr2 = ptr + 2;
    if (ptr2[0] != 'C') return 17;
    if (ptr2[1] != 'D') return 18;
    
    // Test 7: Numeric string
    char *nums = "0123456789";
    if (nums[0] != '0') return 19;
    if (nums[9] != '9') return 20;
    
    // All tests passed!
    return 42;
}
