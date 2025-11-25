// Test union size calculation
union Data {
    int i;
    char c;
    long l;
};

int main() {
    int size = sizeof(union Data);  // Should return 8 (size of long, which is 8 in VM)
    if (size != 8) return 1;  // Assert sizeof(union Data) == 8
    return 42;
}
