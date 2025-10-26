// Debug struct layout
// Expected return: 42

struct Point {
    int x;
    int y;
};

int main() {
    struct Point p;
    p.x = 30;
    p.y = 12;
    return p.x + p.y;
}
