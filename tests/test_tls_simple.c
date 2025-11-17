// Simple TLS test - basic read/write
_Thread_local int tls_var = 42;

int main() {
    if (tls_var != 42) return 1;
    tls_var = 100;
    if (tls_var != 100) return 2;
    return 0;  // Success
}
