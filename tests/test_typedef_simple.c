// Test basic typedef functionality
typedef int Integer;
typedef char Byte;

int main() {
    Integer x = 10;
    Integer y = 32;
    Byte c = 'A';
    
    return x + y;  // Should return 42
}
