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

#include "jcc.h"
#include "./internal.h"
#include <ctype.h>

// ========== FFI Helper ==========

static int find_ffi_function(JCC *vm, const char *name) {
    if (!vm || !name)
        return -1;
    
    // First try exact match
    for (int i = 0; i < vm->compiler.ffi_count; i++) {
        if (strcmp(vm->compiler.ffi_table[i].name, name) == 0) {
            return i;
        }
    }
    
    // If no exact match, check if this looks like a specialized variadic name
    size_t len = strlen(name);
    if ((len > 1 && isdigit(name[len-1])) || 
        (len > 2 && isdigit(name[len-1]) && isdigit(name[len-2]))) {
        char base_name[256];
        strncpy(base_name, name, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
        
        int base_len = len;
        while (base_len > 0 && isdigit(base_name[base_len-1])) {
            base_len--;
        }
        base_name[base_len] = '\0';
        
        for (int i = 0; i < vm->compiler.ffi_count; i++) {
            if (vm->compiler.ffi_table[i].is_variadic && strcmp(vm->compiler.ffi_table[i].name, base_name) == 0) {
                return i;
            }
        }
    }
    
    return -1;
}
// ========== Register Allocator ==========
// Simple bitmap allocator for temporary registers T0-T10

static unsigned int temp_reg_in_use = 0;

static const int temp_reg_map[] = {
    REG_T0, REG_T1, REG_T2, REG_T3, REG_T4,
    REG_T5, REG_T6, REG_T7, REG_T8, REG_T9, REG_T10
};
#define NUM_TEMP_REGS 11

static int alloc_temp_reg(void) {
    for (int i = 0; i < NUM_TEMP_REGS; i++) {
        if (!(temp_reg_in_use & (1 << i))) {
            temp_reg_in_use |= (1 << i);
            return temp_reg_map[i];
        }
    }
    error("codegen: out of temporary registers");
    return -1;
}

static void free_temp_reg(int reg) {
    for (int i = 0; i < NUM_TEMP_REGS; i++) {
        if (temp_reg_map[i] == reg) {
            temp_reg_in_use &= ~(1 << i);
            return;
        }
    }
}

// Mark a specific register as in-use (needed after function calls reset temps)
static void mark_temp_reg_used(int reg) {
    for (int i = 0; i < NUM_TEMP_REGS; i++) {
        if (temp_reg_map[i] == reg) {
            temp_reg_in_use |= (1 << i);
            return;
        }
    }
}

static void reset_temp_regs(void) {
    temp_reg_in_use = 0;
}

// ========== Label Tracking for break/continue ==========

#define MAX_LABELS 256
#define MAX_LABEL_PATCHES 1024

typedef struct {
    char *name;
    long long *address;
} LabelDef;

typedef struct {
    char *name;
    long long *patch_location;
} LabelPatch;

static LabelDef label_defs[MAX_LABELS];
static int num_label_defs = 0;

static LabelPatch label_patches[MAX_LABEL_PATCHES];
static int num_label_patches = 0;

static void reset_labels(void) {
    num_label_defs = 0;
    num_label_patches = 0;
}

// Define a label at the current position
static void define_label(JCC *vm, char *name) {
    if (!name) return;
    if (num_label_defs >= MAX_LABELS) {
        error("codegen: too many labels");
    }
    label_defs[num_label_defs].name = name;
    label_defs[num_label_defs].address = vm->text_ptr + 1;
    num_label_defs++;
}

// Record a jump that needs to be patched later
static void add_label_patch(char *name, long long *patch_location) {
    if (!name) return;
    if (num_label_patches >= MAX_LABEL_PATCHES) {
        error("codegen: too many label patches");
    }
    label_patches[num_label_patches].name = name;
    label_patches[num_label_patches].patch_location = patch_location;
    num_label_patches++;
}

// Patch all forward references to labels
static void patch_labels(void) {
    for (int i = 0; i < num_label_patches; i++) {
        char *name = label_patches[i].name;
        long long *patch = label_patches[i].patch_location;
        
        // Find the label definition
        for (int j = 0; j < num_label_defs; j++) {
            if (strcmp(label_defs[j].name, name) == 0) {
                *patch = (long long)label_defs[j].address;
                break;
            }
        }
    }
}

// ========== Emit Helpers ==========

static void emit(JCC *vm, int instruction) {
    if (!vm || !vm->text_ptr) 
        error("codegen: text segment not initialized");
    *++vm->text_ptr = instruction;
}

static void emit_with_arg(JCC *vm, int instruction, long long arg) {
    emit(vm, instruction);
    *++vm->text_ptr = arg;
}

// 3-register ops: [OP] [rd:8|rs1:8|rs2:8|unused:40]
static void emit_rrr(JCC *vm, int op, int rd, int rs1, int rs2) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RRR(rd, rs1, rs2);
}

// 2-register ops: [OP] [rd:8|rs1:8|unused:48]
static void emit_rr(JCC *vm, int op, int rd, int rs1) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RR(rd, rs1);
}

// 1-register + immediate: [OP] [rd:8|unused:56] [imm:64]
static void emit_ri(JCC *vm, int op, int rd, long long imm) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_R(rd);
    *++vm->text_ptr = imm;
}

// Register + register + immediate: [OP] [rd:8|rs:8|unused:48] [imm:64]
static void emit_rri(JCC *vm, int op, int rd, int rs, long long imm) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RR(rd, rs);
    *++vm->text_ptr = imm;
}

// Float 3-register ops
static void emit_frrr(JCC *vm, int op, int rd, int rs1, int rs2) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RRR(rd, rs1, rs2);
}

// Float 2-register ops (for FNEG3)
static void emit_frr(JCC *vm, int op, int rd, int rs1) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RR(rd, rs1);
}

// ========== Specific Emit Helpers ==========

// LI3: rd = immediate
static void emit_li3(JCC *vm, int rd, long long imm) {
    emit_ri(vm, LI3, rd, imm);
}

// LEA3: rd = bp + offset
static void emit_lea3(JCC *vm, int rd, long long offset) {
    emit_ri(vm, LEA3, rd, offset);
}

// ADDI3: rd = rs + immediate
static void emit_addi3(JCC *vm, int rd, int rs, long long imm) {
    emit_rri(vm, ADDI3, rd, rs, imm);
}

// MOV3: rd = rs
static void emit_mov3(JCC *vm, int rd, int rs) {
    emit_rrr(vm, MOV3, rd, rs, 0);
}

// Load operations based on type
static void emit_load(JCC *vm, Type *ty, int rd, int rs_addr) {
    if (ty->kind == TY_CHAR) {
        emit_rr(vm, LDR_B, rd, rs_addr);
    } else if (ty->kind == TY_SHORT) {
        emit_rr(vm, LDR_H, rd, rs_addr);
    } else if (ty->kind == TY_INT || ty->kind == TY_ENUM) {
        emit_rr(vm, LDR_W, rd, rs_addr);
    } else if (is_flonum(ty)) {
        emit_rr(vm, FLDR, rd, rs_addr);
    } else {
        emit_rr(vm, LDR_D, rd, rs_addr);
    }
}

// Store operations based on type  
static void emit_store(JCC *vm, Type *ty, int rd_val, int rs_addr) {
    if (ty->kind == TY_CHAR || ty->kind == TY_BOOL) {
        emit_rr(vm, STR_B, rd_val, rs_addr);
    } else if (ty->kind == TY_SHORT) {
        emit_rr(vm, STR_H, rd_val, rs_addr);
    } else if (ty->kind == TY_INT || ty->kind == TY_ENUM) {
        emit_rr(vm, STR_W, rd_val, rs_addr);
    } else if (is_flonum(ty)) {
        emit_rr(vm, FSTR, rd_val, rs_addr);
    } else {
        emit_rr(vm, STR_D, rd_val, rs_addr);
    }
}

// JZ3: if rs == 0, jump (returns patch location)
static long long *emit_jz3(JCC *vm, int rs) {
    emit(vm, JZ3);
    *++vm->text_ptr = ENCODE_R(rs);
    long long *patch = ++vm->text_ptr;
    *patch = 0;
    return patch;
}

// JNZ3: if rs != 0, jump (returns patch location)
static long long *emit_jnz3(JCC *vm, int rs) {
    emit(vm, JNZ3);
    *++vm->text_ptr = ENCODE_R(rs);
    long long *patch = ++vm->text_ptr;
    *patch = 0;
    return patch;
}

// ========== Forward Declarations ==========

static void gen_expr(JCC *vm, Node *node, int dest_reg);
static void gen_expr_float(JCC *vm, Node *node, int dest_freg);
static void gen_stmt(JCC *vm, Node *node);
static void gen_addr(JCC *vm, Node *node, int dest_reg);

// ========== Address Generation ==========

// Generate address of an lvalue into dest_reg
static void gen_addr(JCC *vm, Node *node, int dest_reg) {
    switch (node->kind) {
        case ND_VAR:
            if (node->var->is_function) {
                // Function address - emit placeholder and record patch
                emit_ri(vm, LI3, dest_reg, 0);  // Placeholder
                long long *addr_loc = vm->text_ptr;
                
                if (vm->compiler.num_func_addr_patches >= MAX_CALLS) {
                    error("too many function address references");
                }
                vm->compiler.func_addr_patches[vm->compiler.num_func_addr_patches].location = addr_loc;
                vm->compiler.func_addr_patches[vm->compiler.num_func_addr_patches].function = node->var;
                vm->compiler.num_func_addr_patches++;
            } else if (node->var->is_local) {
                // For struct/union parameters, the slot contains a pointer to the struct
                // We need to load that pointer, not the slot address
                if (node->var->is_param && 
                    (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION)) {
                    emit_lea3(vm, dest_reg, node->var->offset);  // Slot address
                    emit_rr(vm, LDR_D, dest_reg, dest_reg);      // Load pointer from slot
                } else {
                    emit_lea3(vm, dest_reg, node->var->offset);
                }
            } else {
                // Global variable
                emit_li3(vm, dest_reg, (long long)(vm->data_seg + node->var->offset));
            }
            return;
            
        case ND_DEREF:
            // Address of *ptr is just ptr
            gen_expr(vm, node->lhs, dest_reg);
            return;
            
        case ND_MEMBER:
            // Address of struct.member = &struct + member_offset
            gen_addr(vm, node->lhs, dest_reg);
            if (node->member->offset != 0) {
                emit_addi3(vm, dest_reg, dest_reg, node->member->offset);
            }
            return;
            
        case ND_COMMA:
            gen_expr(vm, node->lhs, REG_ZERO);  // Discard result
            gen_addr(vm, node->rhs, dest_reg);
            return;
        
        case ND_VLA_PTR:
            // VLA: get the address of the pointer variable itself (for storing into it)
            // NOT the pointer value - that's for gen_expr when accessing the array
            if (node->var->is_local) {
                emit_lea3(vm, dest_reg, node->var->offset);  // Address of the pointer variable
            } else {
                error_tok(vm, node->tok, "VLA must be local");
            }
            return;
            
        default:
            error_tok(vm, node->tok, "not an lvalue");
    }
}

// ========== Expression Generation ==========

// Generate code for expression, result in dest_reg (integer) or dest_freg (float)
static void gen_expr(JCC *vm, Node *node, int dest_reg) {
    if (!node) {
        error("codegen: null expression node");
    }
    
    switch (node->kind) {
        case ND_NULL_EXPR:
            return;
            
        case ND_NUM:
            if (is_flonum(node->ty)) {
                // Float literal - store in data segment and load
                long long offset = vm->data_ptr - vm->data_seg;
                offset = (offset + 7) & ~7;  // Align
                vm->data_ptr = vm->data_seg + offset;
                *(double *)vm->data_ptr = node->fval;
                vm->data_ptr += sizeof(double);
                
                int temp = alloc_temp_reg();
                emit_li3(vm, temp, (long long)(vm->data_seg + offset));
                emit_rr(vm, FLDR, dest_reg, temp);
                free_temp_reg(temp);
            } else {
                emit_li3(vm, dest_reg, node->val);
            }
            return;
            
        case ND_VAR:
            if (node->var->is_function) {
                // Function name used as value - function-to-pointer decay
                // Emit LI3 with placeholder, patch later
                emit_ri(vm, LI3, dest_reg, 0);  // Placeholder
                long long *addr_loc = vm->text_ptr;  // Get the immediate slot
                
                // Record patch location for later resolution
                if (vm->compiler.num_func_addr_patches >= MAX_CALLS) {
                    error("too many function address references");
                }
                vm->compiler.func_addr_patches[vm->compiler.num_func_addr_patches].location = addr_loc;
                vm->compiler.func_addr_patches[vm->compiler.num_func_addr_patches].function = node->var;
                vm->compiler.num_func_addr_patches++;
            } else {
                // For float types, FREG_A0-A7 have the same raw numbers as REG_A0-A7
                // Using dest_reg for address calculation would clobber integer regs
                // Solution: use a temp register for address, then load into dest_reg
                if (is_flonum(node->ty)) {
                    int r_addr = alloc_temp_reg();
                    gen_addr(vm, node, r_addr);
                    emit_load(vm, node->ty, dest_reg, r_addr);
                    free_temp_reg(r_addr);
                } else {
                    gen_addr(vm, node, dest_reg);
                    // For scalars, load the value
                    if (node->ty->kind != TY_ARRAY && 
                        node->ty->kind != TY_STRUCT && 
                        node->ty->kind != TY_UNION) {
                        emit_load(vm, node->ty, dest_reg, dest_reg);
                    }
                }
            }
            return;
            
        case ND_DEREF:
            gen_expr(vm, node->lhs, dest_reg);
            if (node->ty->kind != TY_ARRAY &&
                node->ty->kind != TY_STRUCT &&
                node->ty->kind != TY_UNION) {
                emit_load(vm, node->ty, dest_reg, dest_reg);
            }
            return;
            
        case ND_ADDR:
            gen_addr(vm, node->lhs, dest_reg);
            return;
            
        case ND_NEG:
            gen_expr(vm, node->lhs, dest_reg);
            if (is_flonum(node->ty)) {
                emit_frr(vm, FNEG3, dest_reg, dest_reg);
            } else {
                emit_rr(vm, NEG3, dest_reg, dest_reg);
            }
            return;
            
        case ND_NOT:
            gen_expr(vm, node->lhs, dest_reg);
            emit_rr(vm, NOT3, dest_reg, dest_reg);
            return;
            
        case ND_BITNOT:
            gen_expr(vm, node->lhs, dest_reg);
            emit_rr(vm, BNOT3, dest_reg, dest_reg);
            return;
            
        // Binary arithmetic operations
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_MOD:
        case ND_BITAND:
        case ND_BITOR:
        case ND_BITXOR:
        case ND_SHL:
        case ND_SHR:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE: {
            // Optimization: use dest_reg for LHS to save a temp register
            // Only allocate temp for RHS
            int r_rhs = alloc_temp_reg();
            
            if (is_flonum(node->lhs->ty)) {
                // Float operations
                gen_expr(vm, node->lhs, dest_reg);  // LHS goes directly to dest
                gen_expr(vm, node->rhs, r_rhs);
                
                int fop;
                switch (node->kind) {
                    case ND_ADD: fop = FADD3; break;
                    case ND_SUB: fop = FSUB3; break;
                    case ND_MUL: fop = FMUL3; break;
                    case ND_DIV: fop = FDIV3; break;
                    case ND_EQ:  fop = FEQ3; break;
                    case ND_NE:  fop = FNE3; break;
                    case ND_LT:  fop = FLT3; break;
                    case ND_LE:  fop = FLE3; break;
                    default: error("unsupported float op");
                }
                emit_frrr(vm, fop, dest_reg, dest_reg, r_rhs);
            } else {
                // Integer operations
                gen_expr(vm, node->lhs, dest_reg);  // LHS goes directly to dest
                gen_expr(vm, node->rhs, r_rhs);
                
                int op;
                switch (node->kind) {
                    case ND_ADD:    op = ADD3; break;
                    case ND_SUB:    op = SUB3; break;
                    case ND_MUL:    op = MUL3; break;
                    case ND_DIV:    op = DIV3; break;
                    case ND_MOD:    op = MOD3; break;
                    case ND_BITAND: op = AND3; break;
                    case ND_BITOR:  op = OR3; break;
                    case ND_BITXOR: op = XOR3; break;
                    case ND_SHL:    op = SHL3; break;
                    case ND_SHR:    op = SHR3; break;
                    case ND_EQ:     op = SEQ3; break;
                    case ND_NE:     op = SNE3; break;
                    case ND_LT:     op = SLT3; break;
                    case ND_LE:     op = SLE3; break;
                    default: error("unsupported int op");
                }
                emit_rrr(vm, op, dest_reg, dest_reg, r_rhs);
            }
            
            free_temp_reg(r_rhs);
            return;
        }
        
        case ND_ASSIGN: {
            // IMPORTANT: Evaluate RHS *before* computing LHS address!
            // If RHS is a function call, it will clobber temp registers.
            // Computing LHS address after ensures we get a fresh temp reg.
            
            // For struct/union assignments, we need memcpy (both LHS and RHS are addresses)
            if (node->ty && (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION)) {
                // Struct/union assignment: memcpy from RHS address to LHS address
                int r_src = alloc_temp_reg();
                gen_expr(vm, node->rhs, r_src);  // RHS is struct address
                mark_temp_reg_used(r_src);
                
                int r_dest = alloc_temp_reg();
                gen_addr(vm, node->lhs, r_dest);  // LHS address
                
                // MCPY: REG_A0=dest, REG_A1=src, REG_A2=size
                emit_mov3(vm, REG_A0, r_dest);
                emit_mov3(vm, REG_A1, r_src);
                emit_li3(vm, REG_A2, node->ty->size);
                emit(vm, MCPY);
                
                free_temp_reg(r_src);
                free_temp_reg(r_dest);
                
                // Assignment expression result is the destination address
                if (dest_reg != REG_ZERO) {
                    emit_mov3(vm, dest_reg, REG_A0);
                }
                return;
            }
            
            // First, evaluate RHS into a temporary or dest_reg
            int r_val = dest_reg;
            bool need_free = false;
            if (dest_reg == REG_ZERO) {
                r_val = alloc_temp_reg();
                need_free = true;
            }
            gen_expr(vm, node->rhs, r_val);
            
            // CRITICAL: If RHS contained a function call, reset_temp_regs() was called.
            // We need to re-mark r_val as in-use before allocating r_addr!
            mark_temp_reg_used(r_val);
            
            // Now compute LHS address (after any function calls in RHS are done)
            int r_addr = alloc_temp_reg();
            gen_addr(vm, node->lhs, r_addr);
            
            // Store value to address
            emit_store(vm, node->ty, r_val, r_addr);
            
            free_temp_reg(r_addr);
            if (need_free) {
                free_temp_reg(r_val);
            }
            return;
        }
        
        case ND_COND: {
            // Ternary: cond ? then : else
            int r_cond = alloc_temp_reg();
            gen_expr(vm, node->cond, r_cond);
            long long *jz_else = emit_jz3(vm, r_cond);
            free_temp_reg(r_cond);
            
            gen_expr(vm, node->then, dest_reg);
            emit(vm, JMP);
            long long *jmp_end = ++vm->text_ptr;
            
            *jz_else = (long long)(vm->text_ptr + 1);
            gen_expr(vm, node->els, dest_reg);
            *jmp_end = (long long)(vm->text_ptr + 1);
            return;
        }
        
        case ND_COMMA:
            gen_expr(vm, node->lhs, REG_ZERO);  // Discard
            gen_expr(vm, node->rhs, dest_reg);
            return;
            
        case ND_MEMBER:
            gen_addr(vm, node, dest_reg);
            if (node->ty->kind != TY_ARRAY &&
                node->ty->kind != TY_STRUCT &&
                node->ty->kind != TY_UNION) {
                emit_load(vm, node->ty, dest_reg, dest_reg);
            }
            return;
            
        case ND_CAST:
            gen_expr(vm, node->lhs, dest_reg);
            // Add type conversion if needed
            if (is_flonum(node->ty) && !is_flonum(node->lhs->ty)) {
                // int -> float
                emit_rr(vm, I2F3, dest_reg, dest_reg);
            } else if (!is_flonum(node->ty) && is_flonum(node->lhs->ty)) {
                // float -> int
                emit_rr(vm, F2I3, dest_reg, dest_reg);
            }
            return;
            
        case ND_FUNCALL: {
            // Check if this is a builtin alloca call (used for VLAs)
            if (node->lhs->kind == ND_VAR && node->lhs->var == vm->compiler.builtin_alloca) {
                // Special handling for alloca: uses MALC opcode
                if (!node->args) {
                    error_tok(vm, node->tok, "alloca requires a size argument");
                }
                // Evaluate size argument into REG_A0 (MALC reads from REG_A0)
                reset_temp_regs();
                gen_expr(vm, node->args, REG_A0);
                emit(vm, MALC);  // Size in REG_A0, returns pointer in REG_A0
                if (dest_reg != REG_A0) {
                    emit_mov3(vm, dest_reg, REG_A0);
                }
                return;
            }
            
            // Check if this is setjmp builtin
            if (node->lhs->kind == ND_VAR && node->lhs->var == vm->compiler.builtin_setjmp) {
                if (!node->args) {
                    error_tok(vm, node->tok, "setjmp requires a jmp_buf argument");
                }
                // Evaluate jmp_buf address into REG_A0 (SETJMP reads from REG_A0)
                reset_temp_regs();
                gen_expr(vm, node->args, REG_A0);
                emit(vm, SETJMP);  // Save context, returns 0 in REG_A0
                if (dest_reg != REG_A0) {
                    emit_mov3(vm, dest_reg, REG_A0);
                }
                return;
            }
            
            // Check if this is longjmp builtin
            if (node->lhs->kind == ND_VAR && node->lhs->var == vm->compiler.builtin_longjmp) {
                if (!node->args || !node->args->next) {
                    error_tok(vm, node->tok, "longjmp requires jmp_buf and int arguments");
                }
                // LONGJMP: env in REG_A0, val in REG_A1
                reset_temp_regs();
                gen_expr(vm, node->args, REG_A0);        // env (jmp_buf address)
                gen_expr(vm, node->args->next, REG_A1);  // val
                emit(vm, LONGJMP);  // Restore context and jump (does not return)
                return;
            }
            
            // Check for FFI call - foreign functions use register-based calling convention
            // with operand-based metadata (ffi_idx, nargs, double_arg_mask)
            int ffi_idx = -1;
            if (node->lhs->kind == ND_VAR && node->lhs->var->is_function) {
                ffi_idx = find_ffi_function(vm, node->lhs->var->name);
            }
            
            if (ffi_idx >= 0) {
                // FFI call: args go in REG_A0-A7/FREG_A0-A7, metadata in operands
                reset_temp_regs();
                
                // Count arguments and compute double_arg_mask
                int nargs = 0;
                uint64_t double_arg_mask = 0;
                for (Node *arg = node->args; arg; arg = arg->next) {
                    if (nargs < 64 && is_flonum(arg->ty)) {
                        double_arg_mask |= (1ULL << nargs);
                    }
                    nargs++;
                }
                
                // Evaluate arguments into registers A0-A7
                int arg_idx = 0;
                for (Node *arg = node->args; arg; arg = arg->next) {
                    if (arg_idx < 8) {
                        if (is_flonum(arg->ty)) {
                            gen_expr(vm, arg, FREG_A0 + arg_idx);
                        } else {
                            gen_expr(vm, arg, REG_A0 + arg_idx);
                        }
                    }
                    arg_idx++;
                }
                
                // Emit CALLF with 3 operands: ffi_idx, nargs, double_arg_mask
                emit(vm, CALLF);
                *++vm->text_ptr = ffi_idx;
                *++vm->text_ptr = nargs;
                *++vm->text_ptr = (long long)double_arg_mask;
                
                // Reset temp regs after call
                reset_temp_regs();
                
                // Result in REG_A0/FREG_A0
                if (is_flonum(node->ty)) {
                    if (dest_reg != FREG_A0) {
                        emit_frr(vm, FNEG3, dest_reg, FREG_A0);
                        emit_frr(vm, FNEG3, dest_reg, dest_reg);
                    }
                } else {
                    if (dest_reg != REG_A0) {
                        emit_mov3(vm, dest_reg, REG_A0);
                    }
                }
                return;
            }
            
            // Internal function call: evaluate arguments
            // For variadic functions, varargs (including doubles) go to integer registers
            // so ENT3 can spill them to stack for va_arg to read
            
            bool is_variadic_call = node->func_ty && node->func_ty->is_variadic;
            int fixed_param_count = 0;
            if (is_variadic_call) {
                for (Type *p = node->func_ty->params; p; p = p->next) {
                    fixed_param_count++;
                }
            }
            
            // Track separate int and float register indices
            int int_arg_idx = 0;
            int float_arg_idx = 0;
            int arg_idx = 0;
            
            for (Node *arg = node->args; arg; arg = arg->next, arg_idx++) {
                if (int_arg_idx >= 8 && float_arg_idx >= 8) {
                    // No more registers available - skip
                    // TODO: Support stack args for internal calls
                    continue;
                }
                
                bool is_vararg = is_variadic_call && (arg_idx >= fixed_param_count);
                
                if (is_flonum(arg->ty)) {
                    if (is_vararg) {
                        // Variadic double: put in integer register (as bit pattern)
                        // ENT3 will spill REG_A* to stack; va_arg reads from stack
                        if (int_arg_idx < 8) {
                            // Generate double value into a float reg, then move bits to int reg
                            int freg = FREG_A0;  // Use FREG_A0 as scratch
                            gen_expr(vm, arg, freg);
                            // Move double bits from freg to int reg (bit-pattern, not conversion)
                            emit_rr(vm, FR2R, REG_A0 + int_arg_idx, freg);
                            int_arg_idx++;
                        }
                    } else {
                        // Fixed param double: put in float register
                        if (float_arg_idx < 8) {
                            gen_expr(vm, arg, FREG_A0 + float_arg_idx);
                            float_arg_idx++;
                        }
                    }
                } else {
                    // Integer/pointer argument - always goes in integer register
                    if (int_arg_idx < 8) {
                        gen_expr(vm, arg, REG_A0 + int_arg_idx);
                        int_arg_idx++;
                    }
                }
            }
            
            // Call function
            if (node->lhs->kind == ND_VAR && node->lhs->var->is_function) {
                Obj *fn = node->lhs->var;
                emit(vm, CALL);
                long long *patch = ++vm->text_ptr;
                *patch = 0;  // Will be patched later
                
                // Record call patch location for later resolution
                if (vm->compiler.num_call_patches >= MAX_CALLS) {
                    error("too many function calls");
                }
                vm->compiler.call_patches[vm->compiler.num_call_patches].location = patch;
                vm->compiler.call_patches[vm->compiler.num_call_patches].function = fn;
                vm->compiler.num_call_patches++;
            } else {
                // Indirect call - function pointer in register
                int r_fn = alloc_temp_reg();
                gen_expr(vm, node->lhs, r_fn);
                emit(vm, CALLI);
                *++vm->text_ptr = ENCODE_R(r_fn);
                free_temp_reg(r_fn);
            }
            
            // Function calls clobber all temp registers (caller-saved)
            // Reset allocator so caller will recompute any addresses it needs
            reset_temp_regs();
            
            // Result in REG_A0/FREG_A0
            if (is_flonum(node->ty)) {
                if (dest_reg != FREG_A0) {
                    emit_frr(vm, FNEG3, dest_reg, FREG_A0);  // TODO: Need FMOV3
                    emit_frr(vm, FNEG3, dest_reg, dest_reg);
                }
            } else {
                if (dest_reg != REG_A0) {
                    emit_mov3(vm, dest_reg, REG_A0);
                }
            }
            return;
        }
        
        // ND_MEMZERO is handled in gen_stmt, but can appear in expr context
        case ND_MEMZERO:
            // Zero-initialize memory - typically done at local var declaration
            // For now, just return (handled via assignment)
            return;
        
        case ND_LOGAND: {
            // Logical AND with short-circuit evaluation
            int r_cond = alloc_temp_reg();
            gen_expr(vm, node->lhs, r_cond);
            long long *jz_false = emit_jz3(vm, r_cond);
            
            gen_expr(vm, node->rhs, r_cond);
            long long *jz_false2 = emit_jz3(vm, r_cond);
            
            // Both true
            emit_li3(vm, dest_reg, 1);
            emit(vm, JMP);
            long long *jmp_end = ++vm->text_ptr;
            
            // At least one false
            *jz_false = (long long)(vm->text_ptr + 1);
            *jz_false2 = (long long)(vm->text_ptr + 1);
            emit_li3(vm, dest_reg, 0);
            *jmp_end = (long long)(vm->text_ptr + 1);
            free_temp_reg(r_cond);
            return;
        }
        
        case ND_LOGOR: {
            // Logical OR with short-circuit evaluation
            int r_cond = alloc_temp_reg();
            gen_expr(vm, node->lhs, r_cond);
            long long *jnz_true = emit_jnz3(vm, r_cond);
            
            gen_expr(vm, node->rhs, r_cond);
            long long *jnz_true2 = emit_jnz3(vm, r_cond);
            
            // Both false
            emit_li3(vm, dest_reg, 0);
            emit(vm, JMP);
            long long *jmp_end = ++vm->text_ptr;
            
            // At least one true
            *jnz_true = (long long)(vm->text_ptr + 1);
            *jnz_true2 = (long long)(vm->text_ptr + 1);
            emit_li3(vm, dest_reg, 1);
            *jmp_end = (long long)(vm->text_ptr + 1);
            free_temp_reg(r_cond);
            return;
        }
        
        case ND_STMT_EXPR: {
            // Statement expression ({ stmt; expr })
            // Generate all statements, result of last expression goes to dest_reg
            for (Node *n = node->body; n; n = n->next) {
                if (!n->next && n->kind == ND_EXPR_STMT && n->lhs) {
                    // Last statement - evaluate and keep result
                    gen_expr(vm, n->lhs, dest_reg);
                } else {
                    gen_stmt(vm, n);
                }
            }
            return;
        }
        
        case ND_FRAME_ADDR:
            // __builtin_frame_address(0) - returns current base pointer
            // LEA3 with offset 0 loads bp + 0 = bp into dest_reg
            emit_lea3(vm, dest_reg, 0);
            return;
        
        case ND_VLA_PTR:
            // VLA pointer/designator: load the stored pointer value
            // VLAs are implemented by storing a pointer to dynamically allocated memory
            // The pointer itself is a local variable
            if (node->var->is_local) {
                emit_lea3(vm, dest_reg, node->var->offset);  // Address of pointer
                emit_rr(vm, LDR_D, dest_reg, dest_reg);  // Load the pointer value
            } else {
                error_tok(vm, node->tok, "VLA must be local");
            }
            return;
        
        case ND_LABEL_VAL:
            // Label address: &&label (GCC extension for computed goto)
            // Emit LI3 with placeholder address that will be patched later
            emit_ri(vm, LI3, dest_reg, 0);  // Load immediate with placeholder
            // Get the address slot we just wrote to
            long long *label_addr_loc = vm->text_ptr;
            // Record patch location so it gets resolved when label is defined
            add_label_patch(node->unique_label ? node->unique_label : node->label, label_addr_loc);
            return;
        
        default:
            error_tok(vm, node->tok, "codegen: unsupported expression node kind %d", node->kind);
    }
}

// ========== Statement Generation ==========

static void gen_stmt(JCC *vm, Node *node) {
    if (!node) return;
    
    switch (node->kind) {
        case ND_BLOCK:
            for (Node *n = node->body; n; n = n->next) {
                gen_stmt(vm, n);
            }
            return;
            
        case ND_EXPR_STMT:
            reset_temp_regs();
            gen_expr(vm, node->lhs, REG_ZERO);
            return;
            
        case ND_RETURN:
            reset_temp_regs();
            if (node->lhs) {
                // If returning struct/union, copy to return buffer
                if (node->lhs->ty && (node->lhs->ty->kind == TY_STRUCT || node->lhs->ty->kind == TY_UNION)) {
                    // Get next buffer from pool (rotating)
                    char *buffer = vm->compiler.return_buffer_pool[vm->compiler.return_buffer_index];
                    vm->compiler.return_buffer_index = (vm->compiler.return_buffer_index + 1) % RETURN_BUFFER_POOL_SIZE;
                    
                    // Evaluate source (struct address) into a temp register first
                    int r_src = alloc_temp_reg();
                    gen_expr(vm, node->lhs, r_src);
                    
                    // MCPY uses registers: dest in REG_A0, src in REG_A1, count in REG_A2
                    emit_li3(vm, REG_A0, (long long)buffer);  // dest
                    emit_mov3(vm, REG_A1, r_src);             // src
                    emit_li3(vm, REG_A2, node->lhs->ty->size); // count
                    emit(vm, MCPY);
                    
                    free_temp_reg(r_src);
                    
                    // Return buffer address in REG_A0
                    emit_li3(vm, REG_A0, (long long)buffer);
                } else if (is_flonum(node->lhs->ty)) {
                    gen_expr(vm, node->lhs, FREG_A0);
                } else {
                    gen_expr(vm, node->lhs, REG_A0);
                }
            }
            emit(vm, LEV3);
            return;
            
        case ND_IF: {
            reset_temp_regs();
            int r_cond = alloc_temp_reg();
            gen_expr(vm, node->cond, r_cond);
            long long *jz_else = emit_jz3(vm, r_cond);
            free_temp_reg(r_cond);
            
            gen_stmt(vm, node->then);
            
            if (node->els) {
                emit(vm, JMP);
                long long *jmp_end = ++vm->text_ptr;
                *jz_else = (long long)(vm->text_ptr + 1);
                gen_stmt(vm, node->els);
                *jmp_end = (long long)(vm->text_ptr + 1);
            } else {
                *jz_else = (long long)(vm->text_ptr + 1);
            }
            return;
        }
        
        case ND_FOR: {
            // Init
            if (node->init) {
                gen_stmt(vm, node->init);
            }
            
            long long *loop_start = vm->text_ptr + 1;
            
            // Condition
            long long *jz_end = NULL;
            if (node->cond) {
                reset_temp_regs();
                int r_cond = alloc_temp_reg();
                gen_expr(vm, node->cond, r_cond);
                jz_end = emit_jz3(vm, r_cond);
                free_temp_reg(r_cond);
            }
            
            // Body
            gen_stmt(vm, node->then);
            
            // Define continue label (jumps to increment)
            if (node->cont_label) {
                define_label(vm, node->cont_label);
            }
            
            // Increment
            if (node->inc) {
                reset_temp_regs();
                gen_expr(vm, node->inc, REG_ZERO);
            }
            
            // Jump back to start
            emit_with_arg(vm, JMP, (long long)loop_start);
            
            // Define break label (jumps past loop)
            if (node->brk_label) {
                define_label(vm, node->brk_label);
            }
            
            // Patch exit
            if (jz_end) {
                *jz_end = (long long)(vm->text_ptr + 1);
            }
            return;
        }
        
        case ND_DO: {
            long long *loop_start = vm->text_ptr + 1;
            
            gen_stmt(vm, node->then);
            
            // Define continue label (jumps to condition)
            if (node->cont_label) {
                define_label(vm, node->cont_label);
            }
            
            reset_temp_regs();
            int r_cond = alloc_temp_reg();
            gen_expr(vm, node->cond, r_cond);
            long long *jnz_start = emit_jnz3(vm, r_cond);
            *jnz_start = (long long)loop_start;
            free_temp_reg(r_cond);
            
            // Define break label (jumps past loop)
            if (node->brk_label) {
                define_label(vm, node->brk_label);
            }
            return;
        }
        
        case ND_SWITCH: {
            // Simple switch implementation using linear search
            // Evaluate switch expression
            reset_temp_regs();
            int r_val = alloc_temp_reg();
            gen_expr(vm, node->cond, r_val);
            
            // Count cases and collect case nodes and patch addresses
            #define MAX_SWITCH_CASES 256
            Node *case_nodes[MAX_SWITCH_CASES];
            long long *case_patches[MAX_SWITCH_CASES];
            int num_cases = 0;
            
            // For each case, compare and emit jump (placeholder)
            for (Node *n = node->case_next; n; n = n->case_next) {
                if (num_cases >= MAX_SWITCH_CASES) break;
                case_nodes[num_cases] = n;  // Store node pointer for matching
                
                int r_case = alloc_temp_reg();
                emit_li3(vm, r_case, n->begin);
                emit_rrr(vm, SEQ3, r_case, r_val, r_case);
                case_patches[num_cases] = emit_jnz3(vm, r_case);
                free_temp_reg(r_case);
                num_cases++;
            }
            
            // Jump to default or end
            long long *default_patch = NULL;
            long long *end_patch = NULL;
            emit(vm, JMP);
            if (node->default_case) {
                default_patch = ++vm->text_ptr;
            } else {
                end_patch = ++vm->text_ptr;
            }
            
            // Generate switch body - it's a single block or statement
            // When ND_CASE nodes are encountered during body generation,
            // we need to patch their corresponding jumps
            // Store current case info in compiler state for ND_CASE to use
            Node *saved_case_nodes[MAX_SWITCH_CASES];
            long long *saved_case_patches[MAX_SWITCH_CASES];
            int saved_num_cases = vm->compiler.current_sparse_num;
            for (int i = 0; i < num_cases; i++) {
                saved_case_nodes[i] = vm->compiler.sparse_case_nodes[i];
                saved_case_patches[i] = vm->compiler.sparse_jump_addrs[i];
            }
            
            // Set current switch state
            vm->compiler.current_sparse_num = num_cases;
            for (int i = 0; i < num_cases; i++) {
                vm->compiler.sparse_case_nodes[i] = case_nodes[i];
                vm->compiler.sparse_jump_addrs[i] = case_patches[i];
            }
            Node *saved_default = vm->compiler.current_switch_default;
            long long *saved_default_patch = vm->compiler.current_default_patch;
            vm->compiler.current_switch_default = node->default_case;
            vm->compiler.current_default_patch = default_patch;
            
            // Generate the switch body
            gen_stmt(vm, node->then);
            
            // Restore previous switch state (for nested switches)
            vm->compiler.current_sparse_num = saved_num_cases;
            for (int i = 0; i < saved_num_cases; i++) {
                vm->compiler.sparse_case_nodes[i] = saved_case_nodes[i];
                vm->compiler.sparse_jump_addrs[i] = saved_case_patches[i];
            }
            vm->compiler.current_switch_default = saved_default;
            vm->compiler.current_default_patch = saved_default_patch;
            
            // Patch the break target (end of switch)
            if (node->brk_label) {
                define_label(vm, node->brk_label);
            }
            if (end_patch) {
                *end_patch = (long long)(vm->text_ptr + 1);
            }
            
            free_temp_reg(r_val);
            return;
        }
        
        case ND_CASE: {
            // Case within switch - patch jump address and generate body
            long long target = (long long)(vm->text_ptr + 1);
            
            // Check if this is the default case
            if (node == vm->compiler.current_switch_default) {
                if (vm->compiler.current_default_patch) {
                    *vm->compiler.current_default_patch = target;
                }
            } else {
                // Find this case in the sparse switch table and patch it
                for (int i = 0; i < vm->compiler.current_sparse_num; i++) {
                    if (vm->compiler.sparse_case_nodes[i] == node) {
                        *vm->compiler.sparse_jump_addrs[i] = target;
                        break;
                    }
                }
            }
            
            // Generate the body of this case
            gen_stmt(vm, node->lhs);
            return;
        }
        
        case ND_GOTO:
            // break/continue/goto - emit a jump that will be patched
            if (node->unique_label) {
                // This is a break or continue statement
                emit(vm, JMP);
                long long *patch = ++vm->text_ptr;
                *patch = 0;  // Placeholder
                add_label_patch(node->unique_label, patch);
            } else if (node->label) {
                // Named goto - also needs patching
                emit(vm, JMP);
                long long *patch = ++vm->text_ptr;
                *patch = 0;  // Placeholder
                add_label_patch(node->label, patch);
            }
            return;
        
        case ND_LABEL:
            // Named label statement - define the label and generate the body
            if (node->unique_label) {
                define_label(vm, node->unique_label);
            } else if (node->label) {
                define_label(vm, node->label);
            }
            gen_stmt(vm, node->lhs);
            return;
        
        case ND_ASM:
            // Inline assembly - not supported in VM, skip
            return;
        
        case ND_GOTO_EXPR: {
            // Computed goto: goto *expr
            // Evaluate expression to get target address into a register
            reset_temp_regs();
            int r_target = alloc_temp_reg();
            gen_expr(vm, node->lhs, r_target);
            // Emit JMPI - jump indirect to address in register
            emit(vm, JMPI);
            *++vm->text_ptr = ENCODE_R(r_target);
            free_temp_reg(r_target);
            return;
        }
        
        default:
            error_tok(vm, node->tok, "codegen: unsupported statement node kind %d", node->kind);
    }
}

// ========== Function Generation ==========

void gen_function(JCC *vm, Obj *fn) {
    if (!fn->is_function || !fn->body)
        return;
    
    // Reset label tracking for this function
    reset_labels();
    
    // Count parameters first
    int param_count = 0;
    for (Obj *param = fn->params; param; param = param->next) {
        param_count++;
    }
    
    // For variadic functions, copy all 8 potential arg registers
    bool is_variadic = fn->ty && fn->ty->is_variadic;
    int reg_param_count = is_variadic ? 8 : param_count;
    
    // Stack size starts with space for parameters (at negative offsets)
    int stack_size = reg_param_count;
    
    // Assign parameter offsets (negative, starting at -1)
    // Parameters: bp[-1], bp[-2], ...
    int param_offset = -1;
    for (Obj *param = fn->params; param; param = param->next) {
        param->offset = param_offset;
        param->is_local = true;
        param->is_param = true;
        param_offset--;
    }
    
    // Assign local variable offsets (negative, after params)
    for (Obj *var = fn->locals; var; var = var->next) {
        // Check if this is a parameter (params are also in locals list)
        bool is_param = false;
        for (Obj *p = fn->params; p; p = p->next) {
            if (p == var) {
                is_param = true;
                break;
            }
        }
        
        // Skip builtin variables (va_area and alloca_bottom) and params
        bool is_builtin = (var == fn->va_area) || (var == fn->alloca_bottom);
        
        if (!is_param && !is_builtin) {
            // Calculate how many slots this variable needs
            // VM uses 8-byte slots
            int var_size = 1;
            if (var->ty->kind == TY_ARRAY) {
                var_size = (var->ty->size + 7) / 8;  // Round up to 8-byte slots
            } else if (var->ty->kind == TY_VLA) {
                var_size = 1;  // VLA pointer only
            } else if (var->ty->kind == TY_STRUCT || var->ty->kind == TY_UNION) {
                var_size = (var->ty->size + 7) / 8;  // Round up
            }
            stack_size += var_size;
            var->offset = -stack_size;  // Locals have negative offsets
        }
    }
    
    // Record function address (offset from text_seg start)
    fn->code_addr = (vm->text_ptr + 1 - vm->text_seg);
    
    // Compute float parameter mask for ENT3
    long long float_param_mask = 0;
    int pindex = 0;
    for (Obj *param = fn->params; param && pindex < 8; param = param->next, pindex++) {
        if (is_flonum(param->ty)) {
            float_param_mask |= (1LL << pindex);
        }
    }
    
    // Emit ENT3: [stack_size:32|param_count:32] [float_param_mask]
    long long ent3_operand = ((long long)stack_size) | (((long long)reg_param_count) << 32);
    emit(vm, ENT3);
    *++vm->text_ptr = ent3_operand;
    *++vm->text_ptr = float_param_mask;
    
    // Generate function body
    gen_stmt(vm, fn->body);
    
    // Patch all forward jumps (break/continue/goto)
    patch_labels();
    
    // Implicit return 0 from main
    if (strcmp(fn->name, "main") == 0) {
        emit_li3(vm, REG_A0, 0);
    }
    emit(vm, LEV3);
}

// ========== Top-Level Code Generation ==========

void gen(JCC *vm, Obj *prog) {
    // Reset patch counters
    vm->compiler.num_call_patches = 0;
    vm->compiler.num_func_addr_patches = 0;
    
    // Initialize text pointer - text_seg[0] is reserved for main entry point
    vm->text_ptr = vm->text_seg;
    
    // Initialize global variables in data segment
    for (Obj *var = prog; var; var = var->next) {
        if (!var->is_function) {
            // Align data pointer to 8-byte boundary
            long long offset = vm->data_ptr - vm->data_seg;
            offset = (offset + 7) & ~7;
            vm->data_ptr = vm->data_seg + offset;
            
            // Store the offset in the variable
            var->offset = vm->data_ptr - vm->data_seg;
            
            // Copy init_data if present
            if (var->init_data) {
                memcpy(vm->data_ptr, var->init_data, var->ty->size);
            }
            
            vm->data_ptr += var->ty->size;
        }
    }
    
    // Allocate return buffer pool for struct/union returns at end of data segment
    for (int i = 0; i < RETURN_BUFFER_POOL_SIZE; i++) {
        // Align to 8-byte boundary
        long long offset = vm->data_ptr - vm->data_seg;
        offset = (offset + 7) & ~7;
        vm->data_ptr = vm->data_seg + offset;
        vm->compiler.return_buffer_pool[i] = vm->data_ptr;
        memset(vm->compiler.return_buffer_pool[i], 0, vm->compiler.return_buffer_size);
        vm->data_ptr += vm->compiler.return_buffer_size;
    }
    
    // First pass: Generate code for all functions
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (fn->is_function && fn->body) {
            gen_function(vm, fn);
        }
    }
    
    // Second pass: Patch function call addresses
    for (int i = 0; i < vm->compiler.num_call_patches; i++) {
        char *fn_name = vm->compiler.call_patches[i].function->name;
        long long *loc = vm->compiler.call_patches[i].location;
        
        // Find the function definition in the program list
        Obj *fn_def = NULL;
        for (Obj *fn = prog; fn; fn = fn->next) {
            if (fn->is_function && fn->body && strcmp(fn->name, fn_name) == 0) {
                fn_def = fn;
                break;
            }
        }
        
        if (!fn_def) {
            // Check for FFI function
            int ffi_idx = find_ffi_function(vm, fn_name);
            if (ffi_idx >= 0) {
                // FFI - not handled via CALL, skip
                continue;
            }
            error("undefined function: %s", fn_name);
        }
        
        // code_addr is offset, need to add text_seg base
        long long addr = (long long)(vm->text_seg + fn_def->code_addr);
        *loc = addr;
    }
    
    // Third pass: Patch function address references (for function pointers)
    for (int i = 0; i < vm->compiler.num_func_addr_patches; i++) {
        char *fn_name = vm->compiler.func_addr_patches[i].function->name;
        long long *loc = vm->compiler.func_addr_patches[i].location;
        
        Obj *fn_def = NULL;
        for (Obj *fn = prog; fn; fn = fn->next) {
            if (fn->is_function && fn->body && strcmp(fn->name, fn_name) == 0) {
                fn_def = fn;
                break;
            }
        }
        
        if (fn_def) {
            long long addr = (long long)(vm->text_seg + fn_def->code_addr);
            *loc = addr;
        }
    }
    
    // Find main function and store its address in text_seg[0]
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (fn->is_function && strcmp(fn->name, "main") == 0) {
            vm->text_seg[0] = fn->code_addr;
            return;
        }
    }
    
    error("main() function not found");
}

