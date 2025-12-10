/*
 JCC: JIT C Compiler

 Copyright (C) 2025 George Watson

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
 * ops_new.c - Multi-register opcode implementations
 *
 * These new opcodes work alongside the existing single-accumulator opcodes.
 * Once codegen is fully migrated, the old opcodes can be removed.
 *
 * Instruction encoding:
 *   RRR format: [OPCODE] [rd:8|rs1:8|rs2:8|unused:40]
 *   RI format:  [OPCODE] [rd:8|unused:56] [immediate:64]
 */

#include "jcc.h"
#include "./internal.h"

// ========== Arithmetic Operations ==========

int op_ADD3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long a = vm->regs[rs1];
    long long b = vm->regs[rs2];
    
    // Check for signed addition overflow (only if overflow checks enabled)
    if (vm->flags & JCC_OVERFLOW_CHECKS) {
        if ((b > 0 && a > LLONG_MAX - b) ||
            (b < 0 && a < LLONG_MIN - b)) {
            printf("\n========== INTEGER OVERFLOW ==========\n");
            printf("Addition overflow detected\n");
            printf("Operands: %lld + %lld\n", a, b);
            printf("PC:       0x%llx (offset: %lld)\n",
                    (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
            printf("======================================\n");
            return -1;
        }
    }
    
    if (rd != REG_ZERO)
        vm->regs[rd] = a + b;
    return 0;
}

int op_SUB3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long a = vm->regs[rs1];
    long long b = vm->regs[rs2];
    
    // Check for signed subtraction overflow (only if overflow checks enabled)
    if (vm->flags & JCC_OVERFLOW_CHECKS) {
        if ((b < 0 && a > LLONG_MAX + b) ||
            (b > 0 && a < LLONG_MIN + b)) {
            printf("\n========== INTEGER OVERFLOW ==========\n");
            printf("Subtraction overflow detected\n");
            printf("Operands: %lld - %lld\n", a, b);
            printf("PC:       0x%llx (offset: %lld)\n",
                    (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
            printf("======================================\n");
            return -1;
        }
    }
    
    if (rd != REG_ZERO)
        vm->regs[rd] = a - b;
    return 0;
}

int op_MUL3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long a = vm->regs[rs1];
    long long b = vm->regs[rs2];
    
    // Check for signed multiplication overflow (only if overflow checks enabled)
    if ((vm->flags & JCC_OVERFLOW_CHECKS) && a != 0 && b != 0) {
        if (a == LLONG_MIN || b == LLONG_MIN) {
            // LLONG_MIN * anything except 0, 1, -1 overflows
            if ((a == LLONG_MIN && (b != 0 && b != 1 && b != -1)) ||
                (b == LLONG_MIN && (a != 0 && a != 1 && a != -1))) {
                printf("\n========== INTEGER OVERFLOW ==========\n");
                printf("Multiplication overflow detected\n");
                printf("Operands: %lld * %lld\n", a, b);
                printf("PC:       0x%llx (offset: %lld)\n",
                        (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                printf("======================================\n");
                return -1;
            }
        } else {
            long long result = a * b;
            if (result / a != b) {
                printf("\n========== INTEGER OVERFLOW ==========\n");
                printf("Multiplication overflow detected\n");
                printf("Operands: %lld * %lld\n", a, b);
                printf("PC:       0x%llx (offset: %lld)\n",
                        (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                printf("======================================\n");
                return -1;
            }
        }
    }
    
    if (rd != REG_ZERO)
        vm->regs[rd] = a * b;
    return 0;
}

int op_DIV3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long a = vm->regs[rs1];
    long long b = vm->regs[rs2];
    
    // Check for division by zero
    if (b == 0) {
        printf("\n========== DIVISION BY ZERO ==========\n");
        printf("Attempted division by zero\n");
        printf("Operands: %lld / 0\n", a);
        printf("PC:       0x%llx (offset: %lld)\n",
                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
        printf("======================================\n");
        return -1;
    }
    
    // Check for signed division overflow (LLONG_MIN / -1)
    if (a == LLONG_MIN && b == -1) {
        printf("\n========== INTEGER OVERFLOW ==========\n");
        printf("Division overflow detected\n");
        printf("Operands: %lld / %lld\n", a, b);
        printf("Result would overflow (LLONG_MIN / -1 = LLONG_MAX + 1)\n");
        printf("PC:       0x%llx (offset: %lld)\n",
                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
        printf("======================================\n");
        return -1;
    }
    
    if (rd != REG_ZERO)
        vm->regs[rd] = a / b;
    return 0;
}

int op_MOD3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long a = vm->regs[rs1];
    long long b = vm->regs[rs2];
    
    // Check for modulo by zero
    if (b == 0) {
        printf("\n========== MODULO BY ZERO ==========\n");
        printf("Attempted modulo by zero\n");
        printf("Operands: %lld %% 0\n", a);
        printf("PC:       0x%llx (offset: %lld)\n",
                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
        printf("======================================\n");
        return -1;
    }
    
    if (rd != REG_ZERO)
        vm->regs[rd] = a % b;
    return 0;
}

// ========== Bitwise Operations ==========

int op_AND3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = vm->regs[rs1] & vm->regs[rs2];
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

int op_OR3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = vm->regs[rs1] | vm->regs[rs2];
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

int op_XOR3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = vm->regs[rs1] ^ vm->regs[rs2];
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

int op_SHL3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = vm->regs[rs1] << vm->regs[rs2];
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

int op_SHR3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = vm->regs[rs1] >> vm->regs[rs2];
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

// ========== Comparison Operations ==========

int op_SEQ3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = (vm->regs[rs1] == vm->regs[rs2]) ? 1 : 0;
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

int op_SNE3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = (vm->regs[rs1] != vm->regs[rs2]) ? 1 : 0;
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

int op_SLT3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = (vm->regs[rs1] < vm->regs[rs2]) ? 1 : 0;
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

int op_SGE3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = (vm->regs[rs1] >= vm->regs[rs2]) ? 1 : 0;
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

int op_SGT3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = (vm->regs[rs1] > vm->regs[rs2]) ? 1 : 0;
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

int op_SLE3_fn(JCC *vm) {
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    long long result = (vm->regs[rs1] <= vm->regs[rs2]) ? 1 : 0;
    if (rd != REG_ZERO)
        vm->regs[rd] = result;
    return 0;
}

// ========== Data Movement ==========

int op_LI3_fn(JCC *vm) {
    // Load immediate: [LI3] [rd:8] [immediate:64]
    long long operands = *vm->pc++;
    int rd;
    DECODE_R(operands, rd);
    long long imm = *vm->pc++;
    if (rd != REG_ZERO)
        vm->regs[rd] = imm;
    return 0;
}

int op_MOV3_fn(JCC *vm) {
    // Move register: [MOV3] [rd:8|rs1:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    (void)rs2;  // Unused
    if (rd != REG_ZERO)
        vm->regs[rd] = vm->regs[rs1];
    return 0;
}

// ========== Sync Opcodes (Bridge ax ↔ register file) ==========

int op_AX2R_fn(JCC *vm) {
    // Copy accumulator to register: [AX2R] [rd:8|unused:56]
    long long operands = *vm->pc++;
    int rd;
    DECODE_R(operands, rd);
    if (rd != REG_ZERO)
        vm->regs[rd] = vm->ax;
    return 0;
}

int op_R2AX_fn(JCC *vm) {
    // Copy register to accumulator: [R2AX] [rs1:8|unused:56]
    long long operands = *vm->pc++;
    int rs1;
    DECODE_R(operands, rs1);
    vm->ax = vm->regs[rs1];
    return 0;
}

int op_POP3_fn(JCC *vm) {
    // Pop stack into register: [POP3] [rd:8|unused:56]
    long long operands = *vm->pc++;
    int rd;
    DECODE_R(operands, rd);
    if (rd != REG_ZERO)
        vm->regs[rd] = *vm->sp++;
    return 0;
}

int op_FPOP3_fn(JCC *vm) {
    // Pop stack into float register: [FPOP3] [rd:8|unused:56]
    // The value on stack is the bit pattern of a double (pushed by FPUSH)
    long long operands = *vm->pc++;
    int rd;
    DECODE_R(operands, rd);
    long long bits = *vm->sp++;
    vm->fregs[rd] = *(double *)&bits;
    return 0;
}

// ========== Register-Based Calling Convention ==========

int op_ENT3_fn(JCC *vm) {
    // Enter function: [ENT3] [stack_size:32|param_count:32] [float_param_mask]
    // Creates new stack frame and copies REG_A0-REG_An and FREG_A0-FREG_An to parameter slots
    long long operands = *vm->pc++;
    int stack_size = (int)(operands & 0xFFFFFFFF);
    int param_count = (int)((operands >> 32) & 0xFFFFFFFF);
    long long float_param_mask = *vm->pc++;  // Second operand: which params are floats
    
    // Save old base pointer
    *--vm->sp = (long long)vm->bp;
    vm->bp = vm->sp;
    
    // If stack canaries are enabled, write canary after old bp
    if (vm->flags & JCC_STACK_CANARIES) {
        *--vm->sp = vm->stack_canary;
    }
    
    // Allocate space for local variables AND parameters
    // Parameters are now stored at negative offsets like locals
    vm->sp = vm->sp - stack_size;
    
    // Copy register arguments to their stack slots
    // Parameters are at bp[-1], bp[-2], ... bp[-param_count]
    // Map: REG_Ai/FREG_Ai -> bp[-i-1] depending on float_param_mask
    // We need separate int and float register indices
    int int_reg_idx = 0;
    int float_reg_idx = 0;
    for (int i = 0; i < param_count && i < 8; i++) {
        long long *param_slot = vm->bp - 1 - i;
        if (vm->flags & JCC_STACK_CANARIES) {
            param_slot--;  // Account for canary slot
        }
        
        if (float_param_mask & (1LL << i)) {
            // Float parameter - copy from fregs[]
            // Store double bits as long long
            union { double d; long long ll; } conv;
            conv.d = vm->fregs[FREG_A0 + float_reg_idx];
            *param_slot = conv.ll;
            float_reg_idx++;
        } else {
            // Integer parameter - copy from regs[]
            *param_slot = vm->regs[REG_A0 + int_reg_idx];
            int_reg_idx++;
        }
    }
    
    // Stack overflow checking (for stack instrumentation)
    if (vm->flags & JCC_STACK_INSTR) {
        long long stack_used = (char *)vm->initial_sp - (char *)vm->sp;
        if (stack_used > (long long)(vm->poolsize * sizeof(long long) * 3 / 4)) {
            if (vm->flags & JCC_STACK_INSTR_ERRORS) {
                printf("\n===========================================\n");
                printf("STACK OVERFLOW: Stack usage exceeded 75%% threshold\n");
                printf("  Stack used: %lld bytes\n", stack_used);
                printf("  Stack size: %lld bytes\n", (long long)(vm->poolsize * sizeof(long long)));
                printf("===========================================\n");
                return -1;
            } else if (vm->debug_vm) {
                printf("WARNING: Stack usage %lld bytes exceeds threshold\n", stack_used);
            }
        }
    }
    
    return 0;
}

int op_LEV3_fn(JCC *vm) {
    // Leave function: copies ax to REG_A0 and fax to FREG_A0, restores frame, returns
    // Copy return value to both register files (caller knows which to read)
    vm->regs[REG_A0] = vm->ax;
    vm->fregs[FREG_A0] = vm->fax;
    
    // Restore stack pointer to base pointer
    vm->sp = vm->bp;
    
    // If stack canaries are enabled, check canary
    if (vm->flags & JCC_STACK_CANARIES) {
        long long canary = vm->sp[-1];
        if (canary != vm->stack_canary) {
            printf("\n========== STACK OVERFLOW DETECTED ==========\n");
            printf("Stack canary corrupted!\n");
            printf("Expected: 0x%llx\n", vm->stack_canary);
            printf("Found:    0x%llx\n", canary);
            printf("PC:       0x%llx (offset: %lld)\n",
                    (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
            printf("This indicates a stack buffer overflow.\n");
            printf("=============================================\n");
            return -1;
        }
    }
    
    // Restore old base pointer
    vm->bp = (long long *)*vm->sp++;
    
    // Get return address
    long long ret_addr = *vm->sp++;
    
    // CFI validation
    if (vm->flags & JCC_CFI) {
        long long shadow_ret_addr = *vm->shadow_sp++;
        if (shadow_ret_addr != ret_addr) {
            printf("\n========== CFI VIOLATION ==========\n");
            printf("Control flow integrity violation detected!\n");
            printf("Expected return address: 0x%llx\n", shadow_ret_addr);
            printf("Actual return address:   0x%llx\n", ret_addr);
            printf("Current PC offset:       %lld\n",
                    (long long)(vm->pc - vm->text_seg));
            printf("This indicates a ROP attack or stack corruption.\n");
            printf("====================================\n");
            return -1;
        }
    }
    
    // Check if returning from main (ret_addr == 0)
    if (ret_addr == 0) {
        vm->pc = NULL;  // Signal end of execution
        return 0;
    }
    
    vm->pc = (long long *)ret_addr;
    return 0;
}

// ========== Floating-Point Register File ==========

int op_FAX2FR_fn(JCC *vm) {
    // Copy fax to fregs[rd]: fregs[rd] = fax
    // Format: [FAX2FR] [rd:8|unused:56]
    long long operands = *vm->pc++;
    int rd;
    DECODE_R(operands, rd);
    
    vm->fregs[rd] = vm->fax;
    return 0;
}

int op_FR2FAX_fn(JCC *vm) {
    // Copy fregs[rs1] to fax: fax = fregs[rs1]
    // Format: [FR2FAX] [rs1:8|unused:56]
    long long operands = *vm->pc++;
    int rs1;
    DECODE_R(operands, rs1);
    
    vm->fax = vm->fregs[rs1];
    return 0;
}

// ========== 3-Register Floating-Point Arithmetic ==========

int op_FADD3_fn(JCC *vm) {
    // fregs[rd] = fregs[rs1] + fregs[rs2]
    // Format: [FADD3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    vm->fregs[rd] = vm->fregs[rs1] + vm->fregs[rs2];
    return 0;
}

int op_FSUB3_fn(JCC *vm) {
    // fregs[rd] = fregs[rs1] - fregs[rs2]
    // Format: [FSUB3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    vm->fregs[rd] = vm->fregs[rs1] - vm->fregs[rs2];
    return 0;
}

int op_FMUL3_fn(JCC *vm) {
    // fregs[rd] = fregs[rs1] * fregs[rs2]
    // Format: [FMUL3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    vm->fregs[rd] = vm->fregs[rs1] * vm->fregs[rs2];
    return 0;
}

int op_FDIV3_fn(JCC *vm) {
    // fregs[rd] = fregs[rs1] / fregs[rs2]
    // Format: [FDIV3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    // Division by zero check
    if (vm->fregs[rs2] == 0.0) {
        printf("\n========== DIVISION BY ZERO ==========\n");
        printf("Floating-point division by zero detected!\n");
        printf("PC offset: %lld\n", (long long)(vm->pc - vm->text_seg - 1));
        printf("======================================\n");
        return -1;
    }
    
    vm->fregs[rd] = vm->fregs[rs1] / vm->fregs[rs2];
    return 0;
}

int op_FNEG3_fn(JCC *vm) {
    // fregs[rd] = -fregs[rs1]
    // Format: [FNEG3] [rd:8|rs1:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs1;
    DECODE_RR(operands, rd, rs1);
    
    vm->fregs[rd] = -vm->fregs[rs1];
    return 0;
}

// ========== 3-Register Floating-Point Comparisons ==========

int op_FEQ3_fn(JCC *vm) {
    // regs[rd] = (fregs[rs1] == fregs[rs2])
    // Format: [FEQ3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    vm->regs[rd] = (vm->fregs[rs1] == vm->fregs[rs2]);
    return 0;
}

int op_FNE3_fn(JCC *vm) {
    // regs[rd] = (fregs[rs1] != fregs[rs2])
    // Format: [FNE3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    vm->regs[rd] = (vm->fregs[rs1] != vm->fregs[rs2]);
    return 0;
}

int op_FLT3_fn(JCC *vm) {
    // regs[rd] = (fregs[rs1] < fregs[rs2])
    // Format: [FLT3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    vm->regs[rd] = (vm->fregs[rs1] < vm->fregs[rs2]);
    return 0;
}

int op_FLE3_fn(JCC *vm) {
    // regs[rd] = (fregs[rs1] <= fregs[rs2])
    // Format: [FLE3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    vm->regs[rd] = (vm->fregs[rs1] <= vm->fregs[rs2]);
    return 0;
}

int op_FGT3_fn(JCC *vm) {
    // regs[rd] = (fregs[rs1] > fregs[rs2])
    // Format: [FGT3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    vm->regs[rd] = (vm->fregs[rs1] > vm->fregs[rs2]);
    return 0;
}

int op_FGE3_fn(JCC *vm) {
    // regs[rd] = (fregs[rs1] >= fregs[rs2])
    // Format: [FGE3] [rd:8|rs1:8|rs2:8|unused:40]
    long long operands = *vm->pc++;
    int rd, rs1, rs2;
    DECODE_RRR(operands, rd, rs1, rs2);
    
    vm->regs[rd] = (vm->fregs[rs1] >= vm->fregs[rs2]);
    return 0;
}

// ========== Phase D: Internal Operations ==========

int op_ADDI3_fn(JCC *vm) {
    // Add immediate: regs[rd] = regs[rs1] + immediate
    // Format: [ADDI3] [rd:8|rs1:8|unused:48] [immediate:64]
    // Used for struct offset calculations: struct_addr + offset
    long long operands = *vm->pc++;
    int rd, rs1;
    DECODE_RR(operands, rd, rs1);
    long long imm = *vm->pc++;
    
    if (rd != REG_ZERO)
        vm->regs[rd] = vm->regs[rs1] + imm;
    return 0;
}

int op_NEG3_fn(JCC *vm) {
    // Integer negation: regs[rd] = -regs[rs1]
    // Format: [NEG3] [rd:8|rs1:8|unused:48]
    // Replaces PUSH/-1/MUL pattern for ND_NEG
    long long operands = *vm->pc++;
    int rd, rs1;
    DECODE_RR(operands, rd, rs1);
    
    if (rd != REG_ZERO)
        vm->regs[rd] = -vm->regs[rs1];
    return 0;
}

// ========== Phase C: Register-Based Load/Store ==========

int op_LDR_B_fn(JCC *vm) {
    // Load byte (sign-extend): regs[rd] = *(char*)regs[rs]
    // Format: [LDR_B] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    if (rd != REG_ZERO)
        vm->regs[rd] = *(char *)vm->regs[rs];
    return 0;
}

int op_LDR_H_fn(JCC *vm) {
    // Load halfword (sign-extend): regs[rd] = *(short*)regs[rs]
    // Format: [LDR_H] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    if (rd != REG_ZERO)
        vm->regs[rd] = *(short *)vm->regs[rs];
    return 0;
}

int op_LDR_W_fn(JCC *vm) {
    // Load word (sign-extend): regs[rd] = *(int*)regs[rs]
    // Format: [LDR_W] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    if (rd != REG_ZERO)
        vm->regs[rd] = *(int *)vm->regs[rs];
    return 0;
}

int op_LDR_D_fn(JCC *vm) {
    // Load dword: regs[rd] = *(long long*)regs[rs]
    // Format: [LDR_D] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    if (rd != REG_ZERO)
        vm->regs[rd] = *(long long *)vm->regs[rs];
    return 0;
}

int op_STR_B_fn(JCC *vm) {
    // Store byte: *(char*)regs[rs] = regs[rd]
    // Format: [STR_B] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    *(char *)vm->regs[rs] = (char)vm->regs[rd];
    return 0;
}

int op_STR_H_fn(JCC *vm) {
    // Store halfword: *(short*)regs[rs] = regs[rd]
    // Format: [STR_H] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    *(short *)vm->regs[rs] = (short)vm->regs[rd];
    return 0;
}

int op_STR_W_fn(JCC *vm) {
    // Store word: *(int*)regs[rs] = regs[rd]
    // Format: [STR_W] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    *(int *)vm->regs[rs] = (int)vm->regs[rd];
    return 0;
}

int op_STR_D_fn(JCC *vm) {
    // Store dword: *(long long*)regs[rs] = regs[rd]
    // Format: [STR_D] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    *(long long *)vm->regs[rs] = vm->regs[rd];
    return 0;
}

int op_FLDR_fn(JCC *vm) {
    // Float load: fregs[rd] = *(double*)regs[rs]
    // Format: [FLDR] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    vm->fregs[rd] = *(double *)vm->regs[rs];
    return 0;
}

int op_FSTR_fn(JCC *vm) {
    // Float store: *(double*)regs[rs] = fregs[rd]
    // Format: [FSTR] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    *(double *)vm->regs[rs] = vm->fregs[rd];
    return 0;
}
