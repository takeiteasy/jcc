/*
 JCC Debugger Implementation

 Copyright (C) 2025 George Watson

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#include "jcc.h"

void debugger_init(JCC *vm) {
    vm->enable_debugger = 1;  // Make sure it's enabled
    vm->num_breakpoints = 0;
    vm->single_step = 0;
    vm->step_over = 0;
    vm->step_out = 0;
    vm->step_over_return_addr = NULL;
    vm->step_out_bp = NULL;
    vm->debugger_attached = 0;

    // Initialize all breakpoints
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        vm->breakpoints[i].pc = NULL;
        vm->breakpoints[i].enabled = 0;
        vm->breakpoints[i].hit_count = 0;
        vm->breakpoints[i].condition = NULL;
    }
}

int cc_add_breakpoint(JCC *vm, long long *pc) {
    if (vm->num_breakpoints >= MAX_BREAKPOINTS) {
        printf("Error: Maximum number of breakpoints (%d) reached\n", MAX_BREAKPOINTS);
        return -1;
    }

    // Check if breakpoint already exists at this PC
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (vm->breakpoints[i].enabled && vm->breakpoints[i].pc == pc) {
            printf("Breakpoint already exists at PC %p\n", (void*)pc);
            return i;
        }
    }

    // Find first available slot
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (!vm->breakpoints[i].enabled) {
            vm->breakpoints[i].pc = pc;
            vm->breakpoints[i].enabled = 1;
            vm->breakpoints[i].hit_count = 0;
            vm->breakpoints[i].condition = NULL;
            vm->num_breakpoints++;

            // Calculate offset from text_seg for display
            long long offset = (long long)pc - (long long)vm->text_seg;
            printf("Breakpoint #%d set at PC %p (offset: %lld)\n", i, (void*)pc, offset);
            return i;
        }
    }

    return -1;
}

void cc_remove_breakpoint(JCC *vm, int index) {
    if (index < 0 || index >= MAX_BREAKPOINTS) {
        printf("Error: Invalid breakpoint index %d\n", index);
        return;
    }

    if (!vm->breakpoints[index].enabled) {
        printf("Error: No breakpoint at index %d\n", index);
        return;
    }

    vm->breakpoints[index].enabled = 0;
    vm->breakpoints[index].pc = NULL;
    vm->breakpoints[index].hit_count = 0;
    if (vm->breakpoints[index].condition) {
        free(vm->breakpoints[index].condition);
        vm->breakpoints[index].condition = NULL;
    }
    vm->num_breakpoints--;

    printf("Breakpoint #%d removed\n", index);
}

int debugger_check_breakpoint(JCC *vm) {
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (vm->breakpoints[i].enabled && vm->breakpoints[i].pc == vm->pc) {
            vm->breakpoints[i].hit_count++;
            return 1;
        }
    }
    return 0;
}

void debugger_list_breakpoints(JCC *vm) {
    if (vm->num_breakpoints == 0) {
        printf("No breakpoints set.\n");
        return;
    }

    printf("\nBreakpoints:\n");
    printf("%-5s %-18s %-12s %-10s\n", "Num", "Address", "Offset", "Hit Count");
    printf("%-5s %-18s %-12s %-10s\n", "---", "-------", "------", "---------");

    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (vm->breakpoints[i].enabled) {
            long long offset = (long long)vm->breakpoints[i].pc - (long long)vm->text_seg;
            printf("%-5d 0x%-16llx %-12lld %-10d\n",
                   i,
                   (long long)vm->breakpoints[i].pc,
                   offset,
                   vm->breakpoints[i].hit_count);
        }
    }
    printf("\n");
}

void debugger_print_registers(JCC *vm) {
    printf("\n=== Registers ===\n");
    printf("  ax (int):   0x%016llx (%lld)\n", vm->ax, vm->ax);
    printf("  fax (fp):   %f\n", vm->fax);
    printf("  pc:         %p", (void*)vm->pc);
    if (vm->pc >= vm->text_seg && vm->pc < vm->text_ptr) {
        long long offset = (long long)vm->pc - (long long)vm->text_seg;
        printf(" (offset: %lld)", offset);
    }
    printf("\n");
    printf("  bp:         %p\n", (void*)vm->bp);
    printf("  sp:         %p\n", (void*)vm->sp);
    printf("  cycle:      %lld\n", vm->cycle);
    printf("\n");
}

void debugger_print_stack(JCC *vm, int count) {
    printf("\n=== Stack (top %d entries) ===\n", count);

    long long *sp = vm->sp;
    for (int i = 0; i < count && sp < vm->stack_seg; i++) {
        printf("  sp[%2d] = 0x%016llx  (%lld)\n", i, *sp, *sp);
        sp++;
    }
    printf("\n");
}

static const char* opcode_name(int op) {
    static const char *names[] = {
        "LEA", "IMM", " JMP", "CALL", "CALLI", "JZ", "JNZ", "ENT", "ADJ", "LEV", "LI", "LC", "LS", "LW", "SI", "SC", "SS", "SW", "PUSH",
        "OR", "XOR", "AND", "EQ", "NE", "LT", "GT", "LE", "GE", "SHL", "SHR", "ADD", "SUB", "MUL", "DIV", "MOD",
        // VM memory operations (self-contained, no system calls)
        "MALC", "MFRE", "MCPY",
        // Type conversion instructions
        "SX1", "SX2", "SX4", // Sign extend 1/2/4 bytes to 8 bytes
        "ZX1", "ZX2", "ZX4", // Zero extend 1/2/4 bytes to 8 bytes
        // Floating-point instructions
        "FLD", "FST", "FADD", "FSUB", "FMUL", "FDIV", "FNEG",
        "FEQ", "FNE", "FLT", "FLE", "FGT", "FGE",
        "I2F", "F2I", "FPUSH",
        // Foreign function interface
        "CALLF",
        // Memory safety operations
        "CHKB", "CHKP"
    };

    if (op >= 0 && op < (int)(sizeof(names) / sizeof(names[0])))
        return names[op];
    return "UNKNOWN";
}

void debugger_disassemble_current(JCC *vm) {
    if (!vm->pc || vm->pc < vm->text_seg || vm->pc >= vm->text_ptr) {
        printf("PC out of text segment range\n");
        return;
    }

    long long *pc = vm->pc;
    long long offset = (long long)pc - (long long)vm->text_seg;
    int op = *pc++;

    printf("0x%llx (offset %lld): %s", (long long)vm->pc, offset, opcode_name(op));

    // Print operand for instructions that have one
    switch (op) {
        case LEA:
        case IMM:
        case JMP:
        case CALL:
        case JZ:
        case JNZ:
        case ENT:
        case ADJ:
            printf(" %lld", *pc);
            break;
        default:
            break;
    }
    printf("\n");
}

static void print_help(void) {
    printf("\n=== Debugger Commands ===\n");
    printf("  break/b <offset>   - Set breakpoint at instruction offset\n");
    printf("  delete/d <num>     - Delete breakpoint by number\n");
    printf("  list/l             - List all breakpoints\n");
    printf("  continue/c         - Continue execution\n");
    printf("  step/s             - Single step (into functions)\n");
    printf("  next/n             - Step over (skip function calls)\n");
    printf("  finish/f           - Step out (run until return)\n");
    printf("  registers/r        - Print register values\n");
    printf("  stack/st [count]   - Print stack (default 10 entries)\n");
    printf("  disasm/dis         - Disassemble current instruction\n");
    printf("  memory/m <addr>    - Inspect memory at address\n");
    printf("  help/h/?           - Show this help\n");
    printf("  quit/q             - Exit debugger and program\n");
    printf("\n");
}

void cc_debug_repl(JCC *vm) {
    char line[256];
    char cmd[64];

    vm->debugger_attached = 1;

    printf("\n========================================\n");
    printf("    JCC Debugger\n");
    printf("========================================\n");
    printf("Type 'help' or '?' for command list\n\n");

    debugger_print_registers(vm);
    debugger_disassemble_current(vm);

    while (1) {
        printf("(jcc-dbg) ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Skip empty lines
        if (line[0] == '\0') {
            continue;
        }

        // Parse command
        if (sscanf(line, "%63s", cmd) != 1) {
            continue;
        }

        // Help command
        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
            print_help();
        }
        // Continue
        else if (strcmp(cmd, "continue") == 0 || strcmp(cmd, "c") == 0) {
            vm->single_step = 0;
            vm->step_over = 0;
            vm->step_out = 0;
            break;
        }
        // Single step
        else if (strcmp(cmd, "step") == 0 || strcmp(cmd, "s") == 0) {
            vm->single_step = 1;
            vm->step_over = 0;
            vm->step_out = 0;
            break;
        }
        // Step over
        else if (strcmp(cmd, "next") == 0 || strcmp(cmd, "n") == 0) {
            vm->single_step = 0;
            vm->step_over = 1;
            vm->step_out = 0;
            // Save current return address (on top of stack after CALL)
            if (vm->sp < vm->stack_seg) {
                vm->step_over_return_addr = (long long *)*vm->sp;
            }
            break;
        }
        // Step out
        else if (strcmp(cmd, "finish") == 0 || strcmp(cmd, "f") == 0) {
            vm->single_step = 0;
            vm->step_over = 0;
            vm->step_out = 1;
            vm->step_out_bp = vm->bp;
            break;
        }
        // Print registers
        else if (strcmp(cmd, "registers") == 0 || strcmp(cmd, "r") == 0) {
            debugger_print_registers(vm);
        }
        // Print stack
        else if (strcmp(cmd, "stack") == 0 || strcmp(cmd, "st") == 0) {
            int count = 10;
            sscanf(line, "%*s %d", &count);
            debugger_print_stack(vm, count);
        }
        // Disassemble
        else if (strcmp(cmd, "disasm") == 0 || strcmp(cmd, "dis") == 0) {
            debugger_disassemble_current(vm);
        }
        // Set breakpoint
        else if (strcmp(cmd, "break") == 0 || strcmp(cmd, "b") == 0) {
            long long offset;
            if (sscanf(line, "%*s %lld", &offset) == 1) {
                long long *bp_pc = vm->text_seg + offset;
                if (bp_pc >= vm->text_seg && bp_pc < vm->text_ptr) {
                    cc_add_breakpoint(vm, bp_pc);
                } else {
                    printf("Error: Offset %lld is out of range\n", offset);
                }
            } else {
                printf("Usage: break <offset>\n");
            }
        }
        // Delete breakpoint
        else if (strcmp(cmd, "delete") == 0 || strcmp(cmd, "d") == 0) {
            int num;
            if (sscanf(line, "%*s %d", &num) == 1) {
                cc_remove_breakpoint(vm, num);
            } else {
                printf("Usage: delete <breakpoint_number>\n");
            }
        }
        // List breakpoints
        else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "l") == 0) {
            debugger_list_breakpoints(vm);
        }
        // Memory inspection
        else if (strcmp(cmd, "memory") == 0 || strcmp(cmd, "m") == 0) {
            long long addr;
            if (sscanf(line, "%*s %llx", &addr) == 1) {
                printf("Memory at 0x%llx: 0x%016llx (%lld)\n",
                       addr, *(long long*)addr, *(long long*)addr);
            } else {
                printf("Usage: memory <hex_address>\n");
            }
        }
        // Quit
        else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            printf("Exiting debugger...\n");
            exit(0);
        }
        else {
            printf("Unknown command: %s\n", cmd);
            printf("Type 'help' or '?' for command list\n");
        }
    }

    vm->debugger_attached = 0;
}

int debugger_run(JCC *vm, int argc, char **argv) {
    // Find main function
    Obj *main_fn = NULL;
    for (Obj *obj = vm->globals; obj; obj = obj->next) {
        if (obj->is_function && obj->name && strcmp(obj->name, "main") == 0) {
            main_fn = obj;
            break;
        }
    }

    if (!main_fn) {
        printf("Error: main function not found\n");
        return -1;
    }

    printf("\n========================================\n");
    printf("    JCC Debugger\n");
    printf("========================================\n");
    printf("Starting at entry point (PC: %p, offset: %lld)\n",
            (void*)vm->pc, (long long)(vm->pc - vm->text_seg));
    printf("Type 'help' for commands, 'c' to continue\n\n");

    // Set PC to main
    vm->pc = (long long *)main_fn->code_addr;

    // Setup stack for main(argc, argv)
    vm->sp = vm->stack_seg;
    vm->bp = vm->stack_seg;
    vm->initial_sp = vm->stack_seg;
    vm->initial_bp = vm->stack_seg;

    // Push argv (pointer to array of strings)
    *--vm->sp = (long long)argv;
    // Push argc
    *--vm->sp = argc;
    // Push dummy return address (NULL to detect exit)
    *--vm->sp = 0;

    printf("Starting debugger at main (PC: %p)\n", (void*)vm->pc);
    printf("Type 'help' for debugger commands\n\n");

    // Enter debugger at start
    cc_debug_repl(vm);

    // Main execution loop with debugger support
    // TODO: Integrate with vm_eval to allow stepping and breakpoints

    return 0;
}
