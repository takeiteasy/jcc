// Test union pointers
// Expected return: 42

union Data {
    int i;
    char c;
};

int main() {
    union Data d;
    d.i = 100;
    
    union Data *ptr = &d;
    ptr->i = 42;
    
    return ptr->i;
}
