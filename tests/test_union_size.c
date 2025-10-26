// Test union size calculation
union Data {
    int i;
    char c;
    long l;
};

int main() {
    return sizeof(union Data);  // Should return 8 (size of long, which is 8 in VM)
}
