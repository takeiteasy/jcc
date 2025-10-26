// Test typedef with enums and unions

typedef enum {
    RED,
    GREEN,
    BLUE
} Color;

typedef enum Status {
    INACTIVE,
    ACTIVE = 10,
    PENDING
} Status;

typedef union {
    int i;
    char c;
} Data;

typedef union Value Value;
union Value {
    int x;
    long y;
};

int main() {
    Color c = RED;
    Status s = ACTIVE;
    
    Data d;
    d.i = 42;
    
    Value v;
    v.x = 100;
    
    return d.i;  // 42
}
