// Test struct typedef patterns

// Forward declaration with typedef
typedef struct Point Point;

struct Point {
    int x;
    int y;
};

// Typedef with anonymous struct
typedef struct {
    int width;
    int height;
} Size;

// Typedef of existing struct
struct Color {
    int r;
    int g;
    int b;
};
typedef struct Color Color;

int main() {
    Point p;
    p.x = 10;
    p.y = 20;
    
    Size s;
    s.width = 5;
    s.height = 7;
    
    Color c;
    c.r = 255;
    c.g = 0;
    c.b = 0;
    
    return p.x + s.width + s.height + p.y;  // 10 + 5 + 7 + 20 = 42
}
