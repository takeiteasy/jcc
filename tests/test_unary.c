int main() {
    int a = 5;
    int b = 0;
    
    // Unary operators
    int neg = -a;           // -5
    int not1 = !b;          // 1 (0 is false, so !0 = 1)
    int not2 = !a;          // 0 (5 is true, so !5 = 0)
    int bitnot = ~0;        // -1 (all bits set)
    
    // Result: -5 + 1 + 0 + (-1) = -5
    // But we can't return negative, so let's adjust
    int result = (-neg) + not1 + not2 + (-bitnot);
    // = 5 + 1 + 0 + 1 = 7
    
    return result;
}
