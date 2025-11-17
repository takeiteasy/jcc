// TLS struct test
struct Point { int x; int y; };
_Thread_local struct Point p = {10, 20};

int main() {
    // Test initialization
    if (p.x != 10) return 1;
    if (p.y != 20) return 2;

    // Test write
    p.x = 30;
    p.y = 40;

    if (p.x != 30) return 3;
    if (p.y != 40) return 4;

    // Test pointer to struct
    struct Point *ptr = &p;
    ptr->x = 50;
    ptr->y = 60;

    if (p.x != 50) return 5;
    if (p.y != 60) return 6;

    return 0;  // Success
}
