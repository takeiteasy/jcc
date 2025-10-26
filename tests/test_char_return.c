int main() {
    int i = 1000;
    char c = (char)i;
    int c_as_int = c;
    // Return 0 if c_as_int is -24, otherwise return c_as_int
    if (c_as_int == -24) {
        return 0;
    }
    return c_as_int;
}
