// Test combining strings and arrays

int main() {
    // Array of string pointers
    char *strings[3];
    strings[0] = "First";
    strings[1] = "Second";
    strings[2] = "Third";
    
    // Test accessing first character of each string
    if (strings[0][0] != 'F') return 1;
    if (strings[1][0] != 'S') return 2;
    if (strings[2][0] != 'T') return 3;
    
    // Test accessing other characters
    if (strings[0][4] != 't') return 4;  // "First"[4] = 't'
    if (strings[1][5] != 'd') return 5;  // "Second"[5] = 'd'
    if (strings[2][4] != 'd') return 6;  // "Third"[4] = 'd'
    
    // Character array
    char chars[5];
    chars[0] = 'A';
    chars[1] = 'B';
    chars[2] = 'C';
    chars[3] = 'D';
    chars[4] = '\0';
    
    // Test character array
    if (chars[0] != 'A') return 7;
    if (chars[1] != 'B') return 8;
    if (chars[2] != 'C') return 9;
    if (chars[3] != 'D') return 10;
    if (chars[4] != '\0') return 11;
    
    // Test pointer arithmetic with string from array
    char *ptr = strings[1];  // "Second"
    char *ptr2 = ptr + 3;    // "ond"
    if (ptr2[0] != 'o') return 12;
    if (ptr2[1] != 'n') return 13;
    if (ptr2[2] != 'd') return 14;
    
    return 42;
}
