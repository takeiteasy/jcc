// Just return the first element

int global_array[5] = {10, 20, 30, 40, 50};

int main() {
    int val = global_array[0];
    if (val != 10) return 1;  // Assert val == 10
    return 42;
}
