// Test statement-level error recovery
// This file has 4 stray statement errors
// All should be reported with Level 2 recovery

int main() {
    int x = 1;

    // Error 1: Stray break
    break;

    int y = 2;

    // Error 2: Stray continue
    continue;

    int z = 3;

    // Error 3: Stray case
    case 42:
        x = 10;

    // Error 4: Stray default
    default:
        y = 20;

    return x + y + z;
}
