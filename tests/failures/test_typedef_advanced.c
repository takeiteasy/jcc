// Test nested and chained typedefs

typedef int Integer;
typedef Integer MyInt;      // Typedef of typedef
typedef MyInt *MyIntPtr;    // Typedef of typedef with pointer

typedef struct Point {
    int x;
    int y;
} Point, *PointPtr;         // Multiple typedefs in one declaration

// Typedef of array of pointers
typedef int *PtrArray[3];

// Typedef of pointer to array
typedef int (*ArrayPtr)[5];

// Typedef of function returning pointer
typedef int *(*FuncReturningPtr)(int);

int get_value(int x) {
    return x;
}

int main() {
    // Test chained typedef
    MyInt a = 10;
    MyIntPtr b = &a;
    
    // Test multiple typedef names
    Point p = {5, 7};
    PointPtr pp = &p;
    
    // Verify values
    return a + p.x + p.y + pp->x + pp->y;  // 10 + 5 + 7 + 5 + 7 = 34
}
