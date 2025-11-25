// Debug: Check what value c actually has
int main() {
    int i = 1000;
    char c = (char)i;
    int c_as_int = c;  // Explicit conversion to int
    // Return the value so we can see it
    // If c is -24, returns 256-24 = 232
    // If c is 232, returns 232
    if (c_as_int != 232 && c_as_int != -24) return 1;  // Accept either value
    return 42;
}
