// Test for various literal formats
// Tests: hexadecimal, octal, character escapes
// Expected return: 42

int main() {
    // Hexadecimal literals
    int hex1 = 0xFF;        // 255
    int hex2 = 0x2A;        // 42
    int hex3 = 0x10;        // 16
    int hex4 = 0xA;         // 10
    
    // Octal literals
    int oct1 = 0777;        // 511
    int oct2 = 052;         // 42
    int oct3 = 020;         // 16
    int oct4 = 012;         // 10
    
    // Character literals with hex escapes
    int char_hex1 = '\x41'; // 'A' = 65
    int char_hex2 = '\x2A'; // 42
    int char_hex3 = '\x00'; // null = 0
    
    // Character literals with octal escapes
    int char_oct1 = '\101'; // 'A' = 65
    int char_oct2 = '\052'; // 42
    int char_oct3 = '\000'; // null = 0
    
    // Standard character escapes (already working)
    int newline = '\n';     // 10
    int tab = '\t';         // 9
    int backslash = '\\';   // 92
    int quote = '\'';       // 39
    
    // Test hex literals
    if (hex2 != 42) return 1;
    if (hex1 != 255) return 2;
    if (hex3 != 16) return 3;
    if (hex4 != 10) return 4;
    
    // Test octal literals
    if (oct2 != 42) return 5;
    if (oct1 != 511) return 6;
    if (oct3 != 16) return 7;
    if (oct4 != 10) return 8;
    
    // Test character hex escapes
    if (char_hex2 != 42) return 9;
    if (char_hex1 != 65) return 10;
    if (char_hex3 != 0) return 11;
    
    // Test character octal escapes
    if (char_oct2 != 42) return 12;
    if (char_oct1 != 65) return 13;
    if (char_oct3 != 0) return 14;
    
    // Test standard escapes
    if (newline != 10) return 15;
    if (tab != 9) return 16;
    if (backslash != 92) return 17;
    if (quote != 39) return 18;
    
    // Arithmetic with various literal types
    int result = 0;
    result = hex2;           // 42
    result = result + oct2 - hex2;  // 42 + 42 - 42 = 42
    result = result + char_hex2 - oct2;  // 42 + 42 - 42 = 42
    
    // Mixed base arithmetic
    int mixed = 0x10 + 020 + 10;  // 16 + 16 + 10 = 42
    if (mixed != 42) return 19;
    
    // Character escapes in expressions
    int expr = '\x20' + '\012';  // 32 (hex) + 10 (octal) = 42
    if (expr != 42) return 20;
    
    return result;  // Should return 42
}
