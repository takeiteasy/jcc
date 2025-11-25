// Test just strlen
// Expected return: 42

#include <stdio.h>
#include <string.h>

int main() {
    char *str = "hello world";
    puts(str);  // This works - should print "hello world"
    
    long len;
    len = strlen(str);  // Assign return value
    
    puts("After strlen");
    
    // Try to use len - don't do any arithmetic yet
    if (len) {
        puts("len is nonzero");
    }
    
    return 42;
}
