// Test enum with explicit values and arithmetic
enum Status { 
    INACTIVE = 0,
    ACTIVE = 10,
    PENDING = 20,
    COMPLETE = 30
};

enum Flags {
    FLAG_A = 1,
    FLAG_B = 2,
    FLAG_C = 4,
    FLAG_D = 8
};

int main() {
    enum Status s = ACTIVE;
    enum Flags f = FLAG_B;
    
    // Test arithmetic with enums
    int result = s + f;  // 10 + 2 = 12
    
    // Test comparison
    if (s == ACTIVE && f == FLAG_B) {
        result += COMPLETE;  // 12 + 30 = 42
    }
    
    return result;  // Should return 42
}
