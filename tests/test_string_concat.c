// Test string literal concatenation
// Tests automatic concatenation of adjacent string literals

int main() {
    char *s;
    
    // Test 1: Basic concatenation
    s = "Hello" " " "World";
    if (s[0] != 'H') return 1;
    if (s[6] != 'W') return 2;
    
    // Test 2: Multi-line concatenation
    s = "First"
        "Second";
    if (s[0] != 'F') return 3;
    if (s[5] != 'S') return 4;
    
    // Test 3: Empty string concatenation
    s = "" "test";
    if (s[0] != 't') return 5;
    
    // Test 4: Many concatenations
    s = "a" "b" "c" "d" "e" "f";
    if (s[0] != 'a') return 6;
    if (s[5] != 'f') return 7;
    
    // Test 5: Parenthesized concatenation
    s = ("one" "two");
    if (s[0] != 'o') return 8;
    if (s[3] != 't') return 9;
    
    return 42;  // Success
}
