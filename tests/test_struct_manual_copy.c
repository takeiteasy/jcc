// Test if we can manually copy a struct
struct Point { int x; };

int main() {
    struct Point src;
    struct Point dst;
    
    src.x = 42;
    dst = src;  // Should use MCPY
    
    return dst.x;  // Should return 42
}
