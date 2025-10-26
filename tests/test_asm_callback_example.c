// Example: Using JCC inline assembly callback
// This demonstrates how to register and use a custom asm callback

#include <stdio.h>
#include <string.h>
#include "../src/jcc.h"

// Example callback that prints assembly strings
void example_asm_callback(JCC *vm, const char *asm_str, void *user_data) {
    int *counter = (int*)user_data;
    printf("ASM callback invoked #%d: \"%s\"\n", ++(*counter), asm_str);
    
    // Example: You could parse the assembly string and emit custom bytecode
    // if (strcmp(asm_str, "nop") == 0) {
    //     // Do nothing
    // } else if (strncmp(asm_str, "mov ", 4) == 0) {
    //     // Parse mov instruction and emit VM bytecode
    // }
    
    // Or you could store the assembly for later processing
    // Or you could invoke a JIT compiler
    // Or anything else application-specific
}

// Example callback that emits custom VM instructions
void bytecode_asm_callback(JCC *vm, const char *asm_str, void *user_data) {
    // Example: Recognize specific "pseudo-assembly" and emit VM bytecode
    if (strcmp(asm_str, "push42") == 0) {
        // Emit: IMM 42, PUSH
        *++vm->text_ptr = IMM;
        *++vm->text_ptr = 42;
        *++vm->text_ptr = PUSH;
        printf("Emitted VM bytecode for 'push42'\n");
    } else if (strcmp(asm_str, "pop_and_add") == 0) {
        // Emit: ADD (pops from stack, adds to ax)
        *++vm->text_ptr = ADD;
        printf("Emitted VM bytecode for 'pop_and_add'\n");
    } else {
        printf("Unknown asm directive: \"%s\"\n", asm_str);
    }
}

int main(int argc, char **argv) {
    printf("JCC Inline Assembly Callback Example\n");
    printf("=========================================\n\n");
    
    // Example 1: Simple logging callback
    printf("Example 1: Logging callback\n");
    int asm_count = 0;
    
    JCC vm1;
    cc_init(&vm1, argc, (const char**)argv);
    cc_set_asm_callback(&vm1, example_asm_callback, &asm_count);
    
    // Now when you compile code with asm statements, the callback will be invoked
    printf("Callback registered. When compiling, asm statements will trigger the callback.\n\n");
    
    cc_destroy(&vm1);
    
    // Example 2: Bytecode emission callback
    printf("Example 2: Bytecode emission callback\n");
    
    JCC vm2;
    cc_init(&vm2, argc, (const char**)argv);
    cc_set_asm_callback(&vm2, bytecode_asm_callback, NULL);
    
    printf("Callback registered for custom bytecode emission.\n");
    printf("The callback can emit VM instructions based on asm strings.\n\n");
    
    cc_destroy(&vm2);
    
    // Example 3: No callback (default behavior)
    printf("Example 3: No callback (default)\n");
    
    JCC vm3;
    cc_init(&vm3, argc, (const char**)argv);
    // No callback set - asm statements will be no-ops
    
    printf("No callback registered. asm statements will be ignored (no-op).\n\n");
    
    cc_destroy(&vm3);
    
    printf("Use cases for inline assembly callbacks:\n");
    printf("  - Logging/debugging: Track what assembly is being used\n");
    printf("  - Custom bytecode: Map pseudo-assembly to VM instructions\n");
    printf("  - JIT compilation: Trigger native code generation\n");
    printf("  - Simulation: Model hardware-specific operations\n");
    printf("  - Intrinsics: Implement compiler built-ins via asm syntax\n");
    
    return 0;
}
