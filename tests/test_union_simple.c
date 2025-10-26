// Test basic union functionality
// Expected return: 42

union Data {
    int i;
    char c;
};

int main() {
    union Data d;
    
    // Write as int
    d.i = 42;
    
    // Read as int
    return d.i;
}
