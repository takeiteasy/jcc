// Debug sizeof for flexible array members

struct packet {
    int size;
    char data[];
};

int main() {
    int s = sizeof(struct packet);
    // Return the actual sizeof value to see what it is
    return s;
}
