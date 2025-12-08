int main() { 
    int a = 10; 
    int b = 20;
    int c = a + b;
    // *** c is at offset -3
    // Now we have 3 local variables. When we read c:
    int eq = c == 30;
    return eq;
}
