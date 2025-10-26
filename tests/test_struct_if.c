// Combined test
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
    
    if (result != 42) {
        return 1;
    }
    
    return 42;
}
