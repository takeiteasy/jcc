// Test string literals

int main() {
    char *str1 = "Hello";
    char *str2 = "World";
    
    // Test string comparison and access
    if (str1[0] == 'H' && str1[4] == 'o') {
        if (str2[0] == 'W' && str2[4] == 'd') {
            return 42;
        }
    }
    
    return 0;
}
