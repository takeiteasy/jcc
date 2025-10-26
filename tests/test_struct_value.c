// Test actual value
// Expected return: 42

struct Point {
    int x;
    int y;
};

int main() {
    struct Point p;
    p.x = 10;
    p.y = 32;
    
    int result = p.x + p.y;
    
    return result;
}
