// Direct comparison without intermediate variable

int global_array[5] = {10, 20, 30, 40, 50};

int main() {
    if (global_array[0] != 10) {
        return 1;
    }
    return 42;
}
