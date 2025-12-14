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
    // Leave function: return value already in REG_A0/FREG_A0, restore frame
    // (Caller placed return value in REG_A0 before LEV3)
    
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

// ========== 3-Register Floating-Point Arithmetic ==========

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

// ========== Internal Operations ==========

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

// ========== Register-Based Load/Store ==========

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

// ========== Phase 1: New Register-Based Opcodes ==========

int op_LEA3_fn(JCC *vm) {
    // Load effective address: regs[rd] = bp + immediate
    // Format: [LEA3] [rd:8|unused:56] [immediate:64]
    long long operands = *vm->pc++;
    int rd;
    DECODE_R(operands, rd);
    long long offset = *vm->pc++;
    
    if (rd != REG_ZERO)
        vm->regs[rd] = (long long)(vm->bp + offset);
    return 0;
}

int op_I2F3_fn(JCC *vm) {
    // Int to float: fregs[rd] = (double)regs[rs]
    // Format: [I2F3] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    vm->fregs[rd] = (double)vm->regs[rs];
    return 0;
}

int op_F2I3_fn(JCC *vm) {
    // Float to int: regs[rd] = (long long)fregs[rs]
    // Format: [F2I3] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    if (rd != REG_ZERO)
        vm->regs[rd] = (long long)vm->fregs[rs];
    return 0;
}

int op_FR2R_fn(JCC *vm) {
    // Float register to integer register (bit-pattern transfer, no conversion)
    // Format: [FR2R] [rd:8|rs:8|unused:48]
    // Copies the raw IEEE 754 bits of the double to an integer register
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    if (rd != REG_ZERO) {
        union { double d; long long ll; } conv;
        conv.d = vm->fregs[rs];
        vm->regs[rd] = conv.ll;
    }
    return 0;
}

int op_R2FR_fn(JCC *vm) {
    // Integer register to float register (bit-pattern transfer, no conversion)
    // Format: [R2FR] [rd:8|rs:8|unused:48]
    // Copies the raw bits from integer register to a double (reverse of FR2R)
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    union { double d; long long ll; } conv;
    conv.ll = vm->regs[rs];
    vm->fregs[rd] = conv.d;
    return 0;
}

int op_JZ3_fn(JCC *vm) {
    // Branch if zero: if (regs[rs] == 0) pc = target
    // Format: [JZ3] [rs:8|unused:56] [target:64]
    long long operands = *vm->pc++;
    int rs;
    DECODE_R(operands, rs);
    long long target = *vm->pc++;
    
    if (vm->regs[rs] == 0)
        vm->pc = (long long *)target;
    return 0;
}

int op_JNZ3_fn(JCC *vm) {
    // Branch if non-zero: if (regs[rs] != 0) pc = target
    // Format: [JNZ3] [rs:8|unused:56] [target:64]
    long long operands = *vm->pc++;
    int rs;
    DECODE_R(operands, rs);
    long long target = *vm->pc++;
    
    if (vm->regs[rs] != 0)
        vm->pc = (long long *)target;
    return 0;
}

int op_NOT3_fn(JCC *vm) {
    // Logical not: regs[rd] = !regs[rs]
    // Format: [NOT3] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    if (rd != REG_ZERO)
        vm->regs[rd] = !vm->regs[rs];
    return 0;
}

int op_BNOT3_fn(JCC *vm) {
    // Bitwise not: regs[rd] = ~regs[rs]
    // Format: [BNOT3] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    
    if (rd != REG_ZERO)
        vm->regs[rd] = ~vm->regs[rs];
    return 0;
}

// ========== Register-Based Safety Opcodes ==========

int op_CHKP3_fn(JCC *vm) {
    // Check pointer validity (register-based version of CHKP)
    // Format: [CHKP3] [rs:8|unused:56]
    long long operands = *vm->pc++;
    int rs;
    DECODE_R(operands, rs);
    long long ptr = vm->regs[rs];
    
    if (!(vm->flags & JCC_POINTER_CHECKS)) {
        return 0;
    }
    
    if (ptr == 0) {
        printf("\n========== NULL POINTER DEREFERENCE ==========\n");
        printf("Attempted to dereference NULL pointer\n");
        printf("PC: 0x%llx (offset: %lld)\n",
                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
        printf("============================================\n");
        return -1;
    }
    
    // Check if pointer is in heap range
    if (ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
        // Find allocation header - need to search backwards
        AllocHeader *header = ((AllocHeader *)ptr) - 1;
        
        // Check if freed (UAF detection)
        if ((vm->flags & JCC_UAF_DETECTION) && header->magic == 0xDEADBEEF && header->freed) {
            printf("\n========== USE-AFTER-FREE DETECTED ==========\n");
            printf("Attempted to access freed memory\n");
            printf("Address:     0x%llx\n", ptr);
            printf("Size:        %zu bytes\n", header->size);
            printf("Allocated at PC offset: %lld\n", header->alloc_pc);
            printf("Generation:  %d (freed)\n", header->generation);
            printf("Current PC:  0x%llx (offset: %lld)\n",
                    (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
            printf("============================================\n");
            return -1;
        }
    }
    
    return 0;
}

int op_CHKA3_fn(JCC *vm) {
    // Check pointer alignment (register-based version of CHKA)
    // Format: [CHKA3] [rs:8|unused:56] [alignment:64]
    long long operands = *vm->pc++;
    int rs;
    DECODE_R(operands, rs);
    size_t alignment = (size_t)*vm->pc++;
    long long ptr = vm->regs[rs];
    
    if (!(vm->flags & JCC_ALIGNMENT_CHECKS)) {
        return 0;
    }
    
    if (ptr == 0) {
        return 0;  // NULL will be caught by CHKP3
    }
    
    if (alignment > 1 && (ptr % alignment) != 0) {
        printf("\n========== ALIGNMENT ERROR ==========\n");
        printf("Pointer is misaligned for type\n");
        printf("Address:       0x%llx\n", ptr);
        printf("Required alignment: %zu bytes\n", alignment);
        printf("Current PC:    0x%llx (offset: %lld)\n",
                (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
        printf("=====================================\n");
        return -1;
    }
    
    return 0;
}

int op_CHKT3_fn(JCC *vm) {
    // Check type on dereference (register-based version of CHKT)
    // Format: [CHKT3] [rs:8|unused:56] [expected_type:64]
    long long operands = *vm->pc++;
    int rs;
    DECODE_R(operands, rs);
    int expected_type = (int)*vm->pc++;
    long long ptr = vm->regs[rs];
    
    if (!(vm->flags & JCC_TYPE_CHECKS)) {
        return 0;
    }
    
    if (ptr == 0) {
        return 0;  // NULL will be caught by CHKP3
    }
    
    // Skip check for void* (TY_VOID) and generic pointers (TY_PTR)
    if (expected_type == TY_VOID || expected_type == TY_PTR) {
        return 0;
    }
    
    // Only check heap allocations
    if (ptr >= (long long)vm->heap_seg && ptr < (long long)vm->heap_end) {
        AllocHeader *header = ((AllocHeader *)ptr) - 1;
        
        if (header->magic == 0xDEADBEEF) {
            int actual_type = header->type_kind;
            
            if (actual_type != TY_VOID && actual_type != TY_PTR) {
                if (actual_type != expected_type) {
                    const char *type_names[] = {
                        "void", "bool", "char", "short", "int", "long",
                        "float", "double", "long double", "enum", "pointer",
                        "function", "array", "vla", "struct", "union"
                    };
                    
                    const char *expected_name = (expected_type >= 0 && expected_type < 16)
                        ? type_names[expected_type] : "unknown";
                    const char *actual_name = (actual_type >= 0 && actual_type < 16)
                        ? type_names[actual_type] : "unknown";
                    
                    printf("\n========== TYPE MISMATCH DETECTED ==========\n");
                    printf("Pointer type mismatch on dereference\n");
                    printf("Address:       0x%llx\n", ptr);
                    printf("Expected type: %s\n", expected_name);
                    printf("Actual type:   %s\n", actual_name);
                    printf("Allocated at PC offset: %lld\n", header->alloc_pc);
                    printf("Current PC:    0x%llx (offset: %lld)\n",
                            (long long)vm->pc, (long long)(vm->pc - vm->text_seg));
                    printf("============================================\n");
                    return -1;
                }
            }
        }
    }
    
    return 0;
}

// ========== Control Flow Opcodes ==========

int op_JMP_fn(JCC *vm) {
    vm->pc = (long long *)*vm->pc;
    return 0;
}

int op_CALL_fn(JCC *vm) {
    // Call subroutine: push return address to main stack and shadow stack
    long long target = *vm->pc;  // Target address is operand at current pc
    long long ret_addr = (long long)(vm->pc+1);  // Return after operand
    
    *--vm->sp = ret_addr;
    if (vm->flags & JCC_CFI) {
        *--vm->shadow_sp = ret_addr;  // Also push to shadow stack for CFI
    }
    vm->pc = (long long *)target;
    return 0;
}

int op_CALLI_fn(JCC *vm) {
    // Call indirect: function address in register (read from operand)
    long long operands = *vm->pc++;
    int rs = (int)(operands & 0xFF);
    long long ret_addr = (long long)vm->pc;
    *--vm->sp = ret_addr;
    if (vm->flags & JCC_CFI) {
        *--vm->shadow_sp = ret_addr;
    }
    vm->pc = (long long *)vm->regs[rs];
    return 0;
}

int op_JMPT_fn(JCC *vm) {
    // Jump table: index in REG_A0, *pc contains jump table base address
    long long *jump_table = (long long *)*vm->pc;
    long long target = jump_table[vm->regs[REG_A0]];
    vm->pc = (long long *)target;
    return 0;
}

int op_JMPI_fn(JCC *vm) {
    // Jump indirect - address in register specified by operand
    // Format: [JMPI] [rs:8|unused:56]
    long long operands = *vm->pc++;
    int rs;
    DECODE_R(operands, rs);
    vm->pc = (long long *)vm->regs[rs];
    return 0;
}

int op_ADJ_fn(JCC *vm) {
    vm->sp = vm->sp + *vm->pc++;
    return 0;
}

int op_PSH3_fn(JCC *vm) {
    // Push register value onto stack: *--sp = regs[rs]
    // Format: [PSH3] [rs:8|unused:56]
    long long operands = *vm->pc++;
    int rs;
    DECODE_R(operands, rs);
    *--vm->sp = vm->regs[rs];
    return 0;
}

int op_POP3_fn(JCC *vm) {
    // Pop from stack into register: regs[rd] = *sp++
    // Format: [POP3] [rd:8|unused:56]
    long long operands = *vm->pc++;
    int rd;
    DECODE_R(operands, rd);
    vm->regs[rd] = *vm->sp++;
    return 0;
}

// ========== Type Conversion Opcodes ==========

int op_SX1_fn(JCC *vm) {
    // Sign extend 1 byte to 8 bytes: regs[rd] = (long long)(char)regs[rs]
    // Format: [SX1] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    vm->regs[rd] = (long long)(char)vm->regs[rs];
    return 0;
}

int op_SX2_fn(JCC *vm) {
    // Sign extend 2 bytes to 8 bytes: regs[rd] = (long long)(short)regs[rs]
    // Format: [SX2] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    vm->regs[rd] = (long long)(short)vm->regs[rs];
    return 0;
}

int op_SX4_fn(JCC *vm) {
    // Sign extend 4 bytes to 8 bytes: regs[rd] = (long long)(int)regs[rs]
    // Format: [SX4] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    vm->regs[rd] = (long long)(int)vm->regs[rs];
    return 0;
}

int op_ZX1_fn(JCC *vm) {
    // Zero extend 1 byte to 8 bytes: regs[rd] = (long long)(unsigned char)regs[rs]
    // Format: [ZX1] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    vm->regs[rd] = (long long)(unsigned char)vm->regs[rs];
    return 0;
}

int op_ZX2_fn(JCC *vm) {
    // Zero extend 2 bytes to 8 bytes: regs[rd] = (long long)(unsigned short)regs[rs]
    // Format: [ZX2] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    vm->regs[rd] = (long long)(unsigned short)vm->regs[rs];
    return 0;
}

int op_ZX4_fn(JCC *vm) {
    // Zero extend 4 bytes to 8 bytes: regs[rd] = (long long)(unsigned int)regs[rs]
    // Format: [ZX4] [rd:8|rs:8|unused:48]
    long long operands = *vm->pc++;
    int rd, rs;
    DECODE_RR(operands, rd, rs);
    vm->regs[rd] = (long long)(unsigned int)vm->regs[rs];
    return 0;
}

// ========== Memory Allocation Opcodes ==========

int op_MALC_fn(JCC *vm) {
    // malloc: size in REG_A0, return pointer in REG_A0
    long long requested_size = vm->regs[REG_A0];
    if (requested_size <= 0) {
        vm->regs[REG_A0] = 0;  // Return NULL for invalid size
        return 0;
    }

    // Align to 8-byte boundary
    size_t size = (requested_size + 7) & ~7;
    size_t total_size = size + sizeof(AllocHeader);

    // Check for OOM
    size_t available = (size_t)(vm->heap_end - vm->heap_ptr);
    if (total_size > available) {
        vm->regs[REG_A0] = 0;  // Out of memory
        return 0;
    }

    // Allocate from bump pointer
    AllocHeader *header = (AllocHeader *)vm->heap_ptr;
    header->size = size;
    header->requested_size = requested_size;
    header->magic = 0xDEADBEEF;
    header->freed = 0;
    header->generation = 0;
    header->alloc_pc = vm->text_seg ? (long long)(vm->pc - vm->text_seg) : 0;
    header->type_kind = TY_VOID;

    vm->heap_ptr = vm->heap_ptr + total_size;
    vm->regs[REG_A0] = (long long)(header + 1);  // Return pointer after header

    if (vm->debug_vm) {
        printf("MALC: allocated %zu bytes at 0x%llx\n", size, vm->regs[REG_A0]);
    }
    return 0;
}

int op_MFRE_fn(JCC *vm) {
    // free: pointer in REG_A0
    void *ptr = (void *)vm->regs[REG_A0];
    if (!ptr) {
        return 0;  // free(NULL) is a no-op
    }

    AllocHeader *header = ((AllocHeader *)ptr) - 1;

    // Validate header
    if (header->magic != 0xDEADBEEF) {
        printf("\n========== INVALID FREE ==========\n");
        printf("Pointer does not appear to be from malloc: 0x%llx\n", (long long)ptr);
        printf("===================================\n");
        return -1;
    }

    if (header->freed) {
        printf("\n========== DOUBLE FREE ==========\n");
        printf("Pointer already freed: 0x%llx\n", (long long)ptr);
        printf("===================================\n");
        return -1;
    }

    header->freed = 1;
    header->generation++;

    if (vm->debug_vm) {
        printf("MFRE: freed pointer 0x%llx\n", (long long)ptr);
    }
    return 0;
}

int op_MCPY_fn(JCC *vm) {
    // memcpy: dest in REG_A0, src in REG_A1, count in REG_A2
    void *dest = (void *)vm->regs[REG_A0];
    void *src = (void *)vm->regs[REG_A1];
    size_t count = (size_t)vm->regs[REG_A2];
    memcpy(dest, src, count);
    return 0;
}

int op_REALC_fn(JCC *vm) {
    // realloc: ptr in REG_A0, new_size in REG_A1, return in REG_A0
    void *ptr = (void *)vm->regs[REG_A0];
    long long new_size = vm->regs[REG_A1];

    if (!ptr) {
        // realloc(NULL, size) == malloc(size)
        vm->regs[REG_A0] = new_size;
        return op_MALC_fn(vm);
    }

    if (new_size <= 0) {
        // realloc(ptr, 0) == free(ptr)
        op_MFRE_fn(vm);
        vm->regs[REG_A0] = 0;
        return 0;
    }

    AllocHeader *old_header = ((AllocHeader *)ptr) - 1;
    size_t old_size = old_header->size;

    // Allocate new block
    vm->regs[REG_A0] = new_size;
    int result = op_MALC_fn(vm);
    if (result != 0 || vm->regs[REG_A0] == 0) {
        return result;
    }

    // Copy data
    void *new_ptr = (void *)vm->regs[REG_A0];
    size_t copy_size = old_size < (size_t)new_size ? old_size : (size_t)new_size;
    memcpy(new_ptr, ptr, copy_size);

    // Free old block
    old_header->freed = 1;
    old_header->generation++;

    // Result already in REG_A0
    return 0;
}

int op_CALC_fn(JCC *vm) {
    // calloc: nmemb in REG_A0, size in REG_A1, return in REG_A0
    long long nmemb = vm->regs[REG_A0];
    long long size = vm->regs[REG_A1];
    long long total = nmemb * size;

    vm->regs[REG_A0] = total;
    int result = op_MALC_fn(vm);
    if (result != 0 || vm->regs[REG_A0] == 0) {
        return result;
    }

    // Zero the memory
    memset((void *)vm->regs[REG_A0], 0, total);
    return 0;
}

// ========== Safety Opcodes ==========

int op_CHKB_fn(JCC *vm) {
    // Check array bounds (stub for now)
    vm->pc++;  // Skip operand
    return 0;
}

int op_CHKI_fn(JCC *vm) {
    // Check initialization (stub for now)
    vm->pc++;  // Skip operand
    return 0;
}

int op_MARKI_fn(JCC *vm) {
    // Mark as initialized (stub for now)
    vm->pc++;  // Skip operand
    return 0;
}

int op_MARKA_fn(JCC *vm) {
    // Mark address (stub for now)
    vm->pc += 3;  // Skip 3 operands
    return 0;
}

int op_CHKPA_fn(JCC *vm) {
    // Check pointer arithmetic (stub for now)
    return 0;
}

int op_MARKP_fn(JCC *vm) {
    // Mark provenance (stub for now)
    vm->pc += 3;  // Skip 3 operands
    return 0;
}

int op_SCOPEIN_fn(JCC *vm) {
    // Enter scope (stub for now)
    vm->pc++;  // Skip operand
    return 0;
}

int op_SCOPEOUT_fn(JCC *vm) {
    // Exit scope (stub for now)
    vm->pc++;  // Skip operand
    return 0;
}

int op_CHKL_fn(JCC *vm) {
    // Check liveness (stub for now)
    vm->pc++;  // Skip operand
    return 0;
}

int op_MARKR_fn(JCC *vm) {
    // Mark read (stub for now)
    vm->pc++;  // Skip operand
    return 0;
}

int op_MARKW_fn(JCC *vm) {
    // Mark write (stub for now)
    vm->pc++;  // Skip operand
    return 0;
}

// ========== Setjmp/Longjmp ==========

int op_SETJMP_fn(JCC *vm) {
    // setjmp: jmp_buf address in REG_A0, return 0 in REG_A0
    long long *jmp_buf = (long long *)vm->regs[REG_A0];
    jmp_buf[0] = (long long)vm->pc;
    jmp_buf[1] = (long long)vm->sp;
    jmp_buf[2] = (long long)vm->bp;
    vm->regs[REG_A0] = 0;  // setjmp returns 0 on direct call
    return 0;
}

int op_LONGJMP_fn(JCC *vm) {
    // longjmp: jmp_buf address in REG_A0, value in REG_A1
    long long *jmp_buf = (long long *)vm->regs[REG_A0];
    long long val = vm->regs[REG_A1];
    vm->pc = (long long *)jmp_buf[0];
    vm->sp = (long long *)jmp_buf[1];
    vm->bp = (long long *)jmp_buf[2];
    vm->regs[REG_A0] = val ? val : 1;  // Return value (never 0)
    return 0;
}

// ========== FFI ==========

int op_CALLF_fn(JCC *vm) {
    // Foreign function call using register-based calling convention
    // Operands: [ffi_idx, nargs, double_arg_mask]
    // Arguments: REG_A0-A7 for integers, FREG_A0-A7 for doubles (based on double_arg_mask)
    
    int func_idx = (int)*vm->pc++;
    int actual_nargs = (int)*vm->pc++;
    uint64_t double_arg_mask = (uint64_t)*vm->pc++;
    
    if (func_idx < 0 || func_idx >= vm->compiler.ffi_count) {
        printf("error: invalid FFI function index: %d\n", func_idx);
        return -1;
    }

    ForeignFunc *ff = &vm->compiler.ffi_table[func_idx];
    if (!ff->func_ptr) {
        printf("error: FFI function '%s' not resolved\n", ff->name);
        return -1;
    }

    if (vm->debug_vm)
        printf("CALLF: calling %s with %d args (fixed: %d, variadic: %d, double_mask: 0x%llx)\n",
                ff->name, actual_nargs, ff->num_fixed_args, ff->is_variadic, (unsigned long long)double_arg_mask);

    // Collect arguments from registers
    long long args[8];
    for (int i = 0; i < actual_nargs && i < 8; i++) {
        if (double_arg_mask & (1ULL << i)) {
            // This arg is a double - stored in fregs
            args[i] = *(long long *)&vm->fregs[FREG_A0 + i];
        } else {
            // Integer/pointer arg - stored in regs
            args[i] = vm->regs[REG_A0 + i];
        }
        if (vm->debug_vm)
            printf("  arg[%d] = 0x%llx (%lld) [%s]\n", i, args[i], args[i],
                   (double_arg_mask & (1ULL << i)) ? "double" : "int");
    }

#ifdef JCC_HAS_FFI
    // libffi implementation
    ffi_cif cif;
    ffi_type **arg_types = NULL;
    ffi_type *return_type = ff->returns_double ? &ffi_type_double : &ffi_type_sint64;

    if (actual_nargs > 0) {
        arg_types = malloc(actual_nargs * sizeof(ffi_type *));
        if (!arg_types) {
            printf("error: failed to allocate arg types for FFI\n");
            return -1;
        }

        for (int i = 0; i < actual_nargs; i++) {
            if (i < 64 && (double_arg_mask & (1ULL << i))) {
                arg_types[i] = &ffi_type_double;
            } else {
                arg_types[i] = &ffi_type_sint64;
            }
        }
    }

    ffi_status status;
    if (ff->is_variadic) {
        status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, ff->num_fixed_args,
                                  actual_nargs, return_type, arg_types);
    } else {
        status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, actual_nargs,
                              return_type, arg_types);
    }
    
    if (status != FFI_OK) {
        printf("error: failed to prepare FFI cif (status=%d)\n", status);
        if (arg_types) free(arg_types);
        return -1;
    }

    // Build arg pointers array
    void *arg_ptrs[8];
    for (int i = 0; i < actual_nargs && i < 8; i++) {
        arg_ptrs[i] = &args[i];
    }

    // Call the function
    if (ff->returns_double) {
        double result;
        ffi_call(&cif, FFI_FN(ff->func_ptr), &result, arg_ptrs);
        vm->fregs[FREG_A0] = result;
    } else {
        long long result;
        ffi_call(&cif, FFI_FN(ff->func_ptr), &result, arg_ptrs);
        vm->regs[REG_A0] = result;
    }

    if (arg_types) free(arg_types);

#else
    // Fallback implementation without libffi
#if defined(__aarch64__) || defined(__arm64__)
    // ARM64 inline assembly - uses register calling convention
    int num_fixed = ff->is_variadic ? ff->num_fixed_args : actual_nargs;
    int int_reg_idx = 0;
    int fp_reg_idx = 0;
    int stack_args = 0;

    // Calculate stack args count
    for (int i = 0; i < actual_nargs; i++) {
        int is_double = (i < 64 && (double_arg_mask & (1ULL << i)));
        int is_variadic_arg = (i >= num_fixed);

        if (is_variadic_arg) {
            stack_args++;
        } else if (is_double) {
            if (fp_reg_idx >= 8) stack_args++;
            else fp_reg_idx++;
        } else {
            if (int_reg_idx >= 8) stack_args++;
            else int_reg_idx++;
        }
    }

    // Prepare stack area and registers
    long long stack_area[stack_args > 0 ? stack_args : 1];
    int stack_idx = 0;
    int_reg_idx = 0;
    fp_reg_idx = 0;

    register long long x0 __asm__("x0") = 0;
    register long long x1 __asm__("x1") = 0;
    register long long x2 __asm__("x2") = 0;
    register long long x3 __asm__("x3") = 0;
    register long long x4 __asm__("x4") = 0;
    register long long x5 __asm__("x5") = 0;
    register long long x6 __asm__("x6") = 0;
    register long long x7 __asm__("x7") = 0;
    register double d0 __asm__("d0") = 0.0;
    register double d1 __asm__("d1") = 0.0;
    register double d2 __asm__("d2") = 0.0;
    register double d3 __asm__("d3") = 0.0;
    register double d4 __asm__("d4") = 0.0;
    register double d5 __asm__("d5") = 0.0;
    register double d6 __asm__("d6") = 0.0;
    register double d7 __asm__("d7") = 0.0;

    for (int i = 0; i < actual_nargs && i < 8; i++) {
        int is_double = (i < 64 && (double_arg_mask & (1ULL << i)));
        int is_variadic_arg = (i >= num_fixed);

        if (is_variadic_arg) {
            stack_area[stack_idx++] = args[i];
        } else if (is_double) {
            double val = *(double*)&args[i];
            if (fp_reg_idx < 8) {
                switch(fp_reg_idx++) {
                    case 0: d0 = val; break;
                    case 1: d1 = val; break;
                    case 2: d2 = val; break;
                    case 3: d3 = val; break;
                    case 4: d4 = val; break;
                    case 5: d5 = val; break;
                    case 6: d6 = val; break;
                    case 7: d7 = val; break;
                }
            } else {
                stack_area[stack_idx++] = args[i];
            }
        } else {
            if (int_reg_idx < 8) {
                switch(int_reg_idx++) {
                    case 0: x0 = args[i]; break;
                    case 1: x1 = args[i]; break;
                    case 2: x2 = args[i]; break;
                    case 3: x3 = args[i]; break;
                    case 4: x4 = args[i]; break;
                    case 5: x5 = args[i]; break;
                    case 6: x6 = args[i]; break;
                    case 7: x7 = args[i]; break;
                }
            } else {
                stack_area[stack_idx++] = args[i];
            }
        }
    }

    int stack_bytes = (stack_args * 8 + 15) & ~15;

    if (ff->returns_double) {
        register double result __asm__("d0");
        __asm__ volatile(
            "sub sp, sp, %2\n\t"
            "cbz %2, 2f\n\t"
            "mov x10, sp\n\t"
            "mov x11, %3\n\t"
            "mov x12, %4\n\t"
            "1:\n\t"
            "ldr x13, [x11], #8\n\t"
            "str x13, [x10], #8\n\t"
            "subs x12, x12, #1\n\t"
            "b.ne 1b\n\t"
            "2:\n\t"
            "blr %1\n\t"
            "add sp, sp, %2"
            : "=r"(result)
            : "r"(ff->func_ptr), "r"((long long)stack_bytes), "r"(stack_area), "r"((long long)stack_args),
              "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6), "r"(x7),
              "w"(d0), "w"(d1), "w"(d2), "w"(d3), "w"(d4), "w"(d5), "w"(d6), "w"(d7)
            : "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "memory"
        );
        vm->fregs[FREG_A0] = result;
    } else {
        register long long result __asm__("x0");
        __asm__ volatile(
            "sub sp, sp, %2\n\t"
            "cbz %2, 2f\n\t"
            "mov x10, sp\n\t"
            "mov x11, %3\n\t"
            "mov x12, %4\n\t"
            "1:\n\t"
            "ldr x13, [x11], #8\n\t"
            "str x13, [x10], #8\n\t"
            "subs x12, x12, #1\n\t"
            "b.ne 1b\n\t"
            "2:\n\t"
            "blr %1\n\t"
            "add sp, sp, %2"
            : "=r"(result)
            : "r"(ff->func_ptr), "r"((long long)stack_bytes), "r"(stack_area), "r"((long long)stack_args),
              "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6), "r"(x7),
              "w"(d0), "w"(d1), "w"(d2), "w"(d3), "w"(d4), "w"(d5), "w"(d6), "w"(d7)
            : "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "memory"
        );
        vm->regs[REG_A0] = result;
    }
#else
    #error "FFI inline assembly not implemented for this platform. Build with -DJCC_HAS_FFI to use libffi."
#endif
#endif  // JCC_HAS_FFI
    return 0;
}

// ========== Struct Return Buffer Support ==========

int op_RETBUF_fn(JCC *vm) {
    // Get next return buffer from rotating pool at runtime
    // This ensures chained struct-returning calls (e.g., f(g(), h()))
    // get different buffers automatically
    int idx = vm->runtime_return_buffer_index;
    vm->runtime_return_buffer_index = (idx + 1) % RETURN_BUFFER_POOL_SIZE;
    vm->regs[REG_A0] = (long long)vm->compiler.return_buffer_pool[idx];
    return 0;
}

// ========== Random Canary Generation ==========

long long generate_random_canary(void) {
    long long canary = 0;

#if defined(_WIN32) || defined(_WIN64)
    srand((unsigned int)time(NULL));
    canary = ((long long)rand() << 32) | rand();
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(&canary, sizeof(canary), 1, f) != 1) {
            canary = STACK_CANARY ^ (long long)time(NULL);
        }
        fclose(f);
    } else {
        canary = STACK_CANARY ^ (long long)time(NULL);
    }
#endif

    // Ensure canary has null bytes to make exploitation harder
    canary &= ~0xFF00000000000000LL;
    canary |= 0x00FF000000000000LL;

    return canary;
}
