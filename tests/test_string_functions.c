// Test string operations: length calculation, comparison helper

// Simple string length function
int str_len(char *s) {
    int len = 0;
    while (s[len] != '\0') {
        len = len + 1;
    }
    return len;
}

// Simple string comparison (returns 1 if equal, 0 if not)
int str_equal(char *s1, char *s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) {
            return 0;
        }
        i = i + 1;
    }
    // Check both ended at same time
    if (s1[i] == '\0' && s2[i] == '\0') {
        return 1;
    }
    return 0;
}

int main() {
    // Test string length
    char *test1 = "Hello";
    if (str_len(test1) != 5) return 1;
    
    char *test2 = "";
    if (str_len(test2) != 0) return 2;
    
    char *test3 = "A";
    if (str_len(test3) != 1) return 3;
    
    char *test4 = "This is a longer string!";
    if (str_len(test4) != 24) return 4;
    
    // Test string equality
    char *str1 = "ABC";
    char *str2 = "ABC";
    // Note: These may be different addresses
    // but should have same content
    if (str1[0] != 'A' || str1[1] != 'B' || str1[2] != 'C') return 5;
    if (str2[0] != 'A' || str2[1] != 'B' || str2[2] != 'C') return 6;
    
    char *str3 = "XYZ";
    if (str3[0] == 'A') return 7;  // Should be different
    
    return 42;
}
