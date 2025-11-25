// Simple FFI test
// Expected return: 42

#include <stdio.h>
#include <string.h>

int main() {
    // Test strlen
    char *str = "hello";
    puts(str);  // Should print "hello"
    
    int len = strlen(str);
    putchar('0' + len);  // Should print length as digit
    putchar('\n');
    
    if (len != 5) {
        puts("strlen failed");
        return 1;
    }
    
    // Test strcmp
    char *s1 = "hello";
    char *s2 = "hello";
    int cmp = strcmp(s1, s2);
    
    if (cmp != 0) {
        puts("strcmp failed");
        return 2;
    }
    
    puts("All tests passed!");
    return 42;
}
