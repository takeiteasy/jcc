// Debug sizeof for flexible array members

struct packet {
    int size;
    char data[];
};

int main() {
    int s = sizeof(struct packet);
    // Return the actual sizeof value to see what it is
    if (s != 4) return 1;  // Assert s == 4 (sizeof(int))
    return 42;
}
