int main() {
    int a = 10;
    int b = 20;
    
    // Test comparisons (1 = true, 0 = false)
    int eq = (a == 10);     // 1
    int ne = (a != b);      // 1
    int lt = (a < b);       // 1
    int le = (a <= 10);     // 1
    int gt = (b > a);       // 1 (converted to a < b)
    int ge = (b >= 20);     // 1 (converted to 20 <= b)
    
    // Sum should be 6
    int result = eq + ne + lt + le + gt + ge;
    
    return result;
}
