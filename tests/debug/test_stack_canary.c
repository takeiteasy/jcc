// Test for stack canary detection
// This should trigger a stack overflow error when -stack-canaries is used

int vulnerable_function() {
    char buffer[8];

    // Write just past the buffer to corrupt the canary
    // (but not so much that we segfault the actual C process)
    buffer[8] = 'A';   // One byte past the buffer
    buffer[9] = 'B';
    buffer[10] = 'C';
    buffer[11] = 'D';
    buffer[12] = 'E';
    buffer[13] = 'F';
    buffer[14] = 'G';
    buffer[15] = 'H';

    return 42;
}

int main() {
    vulnerable_function();
    return 0;
}
