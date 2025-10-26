// Test for string initialization of char arrays
// Tests: char str[] = "hello"; syntax
// Expected return: 42

int main() {
    // Test 1: Basic string initialization
    char str1[] = "hello";
    if (str1[0] != 'h') return 1;
    if (str1[1] != 'e') return 2;
    if (str1[2] != 'l') return 3;
    if (str1[3] != 'l') return 4;
    if (str1[4] != 'o') return 5;
    if (str1[5] != '\0') return 6;  // Null terminator
    
    // Test 2: Empty string
    char str2[] = "";
    if (str2[0] != '\0') return 7;
    
    // Test 3: Single character string
    char str3[] = "A";
    if (str3[0] != 'A') return 8;
    if (str3[1] != '\0') return 9;
    
    // Test 4: String with escape sequences
    char str4[] = "a\nb";
    if (str4[0] != 'a') return 10;
    if (str4[1] != '\n') return 11;
    if (str4[2] != 'b') return 12;
    if (str4[3] != '\0') return 13;
    
    // Test 5: Calculate length manually
    char str5[] = "test";
    int len = 0;
    while (str5[len] != '\0') {
        len = len + 1;
    }
    if (len != 4) return 14;
    
    // Test 6: String with numbers
    char str6[] = "123";
    if (str6[0] != '1') return 15;
    if (str6[1] != '2') return 16;
    if (str6[2] != '3') return 17;
    
    // Test 7: Modify char array
    char str7[] = "xyz";
    str7[0] = 'a';
    str7[1] = 'b';
    str7[2] = 'c';
    if (str7[0] != 'a') return 18;
    if (str7[1] != 'b') return 19;
    if (str7[2] != 'c') return 20;
    
    // Test 8: Array size calculation
    char str8[] = "hello world";
    // Size should be 12 (11 chars + null terminator)
    int count = 0;
    for (int i = 0; str8[i] != '\0'; i = i + 1) {
        count = count + 1;
    }
    if (count != 11) return 21;
    
    // Test 9: String comparison (manual)
    char str9[] = "ab";
    char str10[] = "ab";
    if (str9[0] != str10[0]) return 22;
    if (str9[1] != str10[1]) return 23;
    
    // Test 10: Calculate ASCII sum for result
    char str11[] = "*";  // ASCII 42
    int result = str11[0];
    
    return result;  // Should return 42 (ASCII of '*')
}
