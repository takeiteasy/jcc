// Test: VLA with expression for size
// Expected return: 84

int main() {
    int x = 3;
    int y = 4;
    int arr[x + y];  // 7 elements
    
    for (int i = 0; i < 7; i = i + 1) {
        arr[i] = i * 10;
    }
    
    int sum = 0;
    for (int i = 0; i < 7; i = i + 1) {
        sum = sum + arr[i];
    }
    
    // sum = 0+10+20+30+40+50+60 = 210
    return sum / 2 - 21;  // 210/2 - 21 = 105 - 21 = 84
}
