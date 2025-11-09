// Test for CFI violation - stack corruption simulation
// This should trigger a CFI violation error when -C flag is used

void corrupt_stack() {
    // Simulate stack corruption by writing to the return address on the stack
    // In the VM, the return address is stored on the stack above the base pointer
    // Stack layout during function execution:
    // [ret_addr] [old_bp] [locals...] <- sp
    //             ^ bp points here

    // Get a pointer to our stack frame
    long long *bp_ptr = (long long *)&bp_ptr;  // Approximate bp location

    // The return address is typically at bp+1 in the VM's stack layout
    // This is a heuristic and may not always work, but demonstrates the concept
    // We'll try to corrupt memory around the stack area

    // Attempt to write to what we think is the return address location
    // (This is highly architecture and implementation dependent)
    // For demonstration purposes, we'll use a buffer overflow

    char buffer[8];
    // Overflow the buffer to corrupt the stack
    // This writes past the buffer into the stack frame
    for (int i = 0; i < 32; i++) {
        buffer[i] = 0x42;  // Corrupt stack with arbitrary values
    }
}

int main() {
    corrupt_stack();
    return 42;  // Should not reach here if CFI detects the corruption
}
