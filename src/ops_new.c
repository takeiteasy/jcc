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
