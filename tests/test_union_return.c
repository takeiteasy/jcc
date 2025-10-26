// Union return test  
// Expected return: 42

union Data {
    int i;
};

union Data make_data() {
    union Data d;
    d.i = 42;
    return d;
}

int main() {
    union Data result = make_data();
    return result.i;
}
