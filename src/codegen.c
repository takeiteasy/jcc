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

static void emit(JCC *vm, int instruction) {
    if (!vm || !vm->text_ptr) 
        error("codegen: text segment not initialized");
    *++vm->text_ptr = instruction;
}

static void emit_with_arg(JCC *vm, int instruction, long long arg) {
    emit(vm, instruction);
    *++vm->text_ptr = arg;
}

// ========== Multi-Register Emit Helpers ==========
// Emit 3-register instruction: [OPCODE] [rd:8|rs1:8|rs2:8|unused:40]
static void emit_rrr(JCC *vm, int op, int rd, int rs1, int rs2) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RRR(rd, rs1, rs2);
}

// Sync helpers for bridging ax ↔ register file
static void emit_ax2r(JCC *vm, int rd) {
    emit(vm, AX2R);
    *++vm->text_ptr = ENCODE_R(rd);
}

static void emit_r2ax(JCC *vm, int rs) {
    emit(vm, R2AX);
    *++vm->text_ptr = ENCODE_R(rs);
}

static void emit_pop3(JCC *vm, int rd) {
    emit(vm, POP3);
    *++vm->text_ptr = ENCODE_R(rd);
}

static void emit_fpop3(JCC *vm, int rd) {
    emit(vm, FPOP3);
    *++vm->text_ptr = ENCODE_R(rd);
}

// Float register sync helpers
static void emit_fax2fr(JCC *vm, int rd) {
    emit(vm, FAX2FR);
    *++vm->text_ptr = ENCODE_R(rd);
}

// Move fax bits to integer register (for passing float varargs in int regs)
// Uses FPUSH to push fax bits to stack, then POP3 to pop into int reg
static void emit_fax2r(JCC *vm, int rd) {
    emit(vm, FPUSH);   // Push fax bits to stack
    emit_pop3(vm, rd); // Pop to integer register
}

static void emit_fr2fax(JCC *vm, int rs) {
    emit(vm, FR2FAX);
    *++vm->text_ptr = ENCODE_R(rs);
}

// Float 3-register operation: fregs[rd] = fregs[rs1] OP fregs[rs2]
static void emit_frrr(JCC *vm, int op, int rd, int rs1, int rs2) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RRR(rd, rs1, rs2);
}

// Float 2-register operation: fregs[rd] = OP fregs[rs1] (for FNEG3)
static void emit_frr(JCC *vm, int op, int rd, int rs1) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RR(rd, rs1);
}

// Integer 2-register operation: regs[rd] = OP regs[rs1] (for NEG3)
static void emit_rr(JCC *vm, int op, int rd, int rs1) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RR(rd, rs1);
}

// Register + register + immediate: regs[rd] = regs[rs1] + imm (for ADDI3)
static void emit_rri(JCC *vm, int op, int rd, int rs1, long long imm) {
    emit(vm, op);
    *++vm->text_ptr = ENCODE_RR(rd, rs1);
    *++vm->text_ptr = imm;
}

// ========== Phase C: Register-Based Load/Store Helpers ==========

// Emit register-based load: regs[rd] = *regs[rs] (typed by size)
// Returns the opcode used for the load
static void emit_load3(JCC *vm, Type *ty, int rd, int rs) {
    if (ty->kind == TY_CHAR) {
        emit_rr(vm, LDR_B, rd, rs);  // 1 byte, sign-extend
    } else if (ty->kind == TY_SHORT) {
        emit_rr(vm, LDR_H, rd, rs);  // 2 bytes, sign-extend
    } else if (ty->kind == TY_INT || ty->kind == TY_ENUM) {
        emit_rr(vm, LDR_W, rd, rs);  // 4 bytes, sign-extend
    } else if (is_flonum(ty)) {
        emit_rr(vm, FLDR, rd, rs);   // float/double to fregs
    } else {
        emit_rr(vm, LDR_D, rd, rs);  // 8 bytes (long, pointers)
    }
}

// Emit register-based store: *regs[rs_addr] = regs[rs_val] (typed by size)
static void emit_store3(JCC *vm, Type *ty, int rs_val, int rs_addr) {
    if (ty->kind == TY_CHAR) {
        emit_rr(vm, STR_B, rs_val, rs_addr);  // 1 byte
    } else if (ty->kind == TY_SHORT) {
        emit_rr(vm, STR_H, rs_val, rs_addr);  // 2 bytes
    } else if (ty->kind == TY_INT || ty->kind == TY_ENUM) {
        emit_rr(vm, STR_W, rs_val, rs_addr);  // 4 bytes
    } else if (is_flonum(ty)) {
        emit_rr(vm, FSTR, rs_val, rs_addr);   // float/double from fregs
    } else {
        emit_rr(vm, STR_D, rs_val, rs_addr);  // 8 bytes (long, pointers)
    }
}

// Emit register-based load with optional security checks
// rs_addr: register containing the address to load from
// rd: destination register for loaded value
// is_deref: true if this is a pointer dereference (triggers security checks)
static void emit_load3_checked(JCC *vm, Type *ty, int rd, int rs_addr, int is_deref) {
    // Security checks operate on the address register
    // For now, we need to sync to ax for the legacy check opcodes
    // TODO: Add register-based CHKP3/CHKA3/CHKT3 opcodes later
    if (is_deref && (vm->flags & JCC_POINTER_CHECKS)) {
        emit_r2ax(vm, rs_addr);  // Sync address to ax for CHKP
        emit(vm, CHKP);  // Check that pointer is valid
    }

    if (is_deref && (vm->flags & JCC_ALIGNMENT_CHECKS)) {
        size_t type_size = ty->size;
        if (type_size > 1) {
            emit_r2ax(vm, rs_addr);  // Sync address to ax for CHKA
            emit_with_arg(vm, CHKA, type_size);
        }
    }

    if (is_deref && (vm->flags & JCC_TYPE_CHECKS)) {
        emit_r2ax(vm, rs_addr);  // Sync address to ax for CHKT
        emit_with_arg(vm, CHKT, ty->kind);
    }

    // Now do the actual load using registers
    emit_load3(vm, ty, rd, rs_addr);
}
// Note: Legacy emit_load() and emit_store() removed - all load/store ops now use
// register-based emit_load3(), emit_load3_checked(), and emit_store3() helpers


// Emit debug info (source location mapping) for debugger
static void emit_debug_info(JCC *vm, Token *tok) {
    if (!(vm->flags & JCC_ENABLE_DEBUGGER) || !tok || !tok->file) {
        return;
    }

    // Only emit if line or column changed
    if (tok->file == vm->dbg.last_debug_file &&
        tok->line_no == vm->dbg.last_debug_line &&
        tok->col_no == vm->dbg.last_debug_col) {
        return;
    }

    // Grow source map if needed
    if (vm->dbg.source_map_count >= vm->dbg.source_map_capacity) {
        vm->dbg.source_map_capacity *= 2;
        SourceMap *new_map = realloc(vm->dbg.source_map, vm->dbg.source_map_capacity * sizeof(SourceMap));
        if (!new_map) {
            error("could not realloc source map");
        }
        vm->dbg.source_map = new_map;
    }

    // Add mapping entry
    vm->dbg.source_map[vm->dbg.source_map_count].pc_offset = vm->text_ptr - vm->text_seg;
    vm->dbg.source_map[vm->dbg.source_map_count].file = tok->file;
    vm->dbg.source_map[vm->dbg.source_map_count].line_no = tok->line_no;
    vm->dbg.source_map[vm->dbg.source_map_count].col_no = tok->col_no;
    // Calculate end column using token length
    vm->dbg.source_map[vm->dbg.source_map_count].end_col_no = tok->col_no + display_width(vm, tok->loc, tok->len);
    vm->dbg.source_map_count++;

    vm->dbg.last_debug_file = tok->file;
    vm->dbg.last_debug_line = tok->line_no;
    vm->dbg.last_debug_col = tok->col_no;
}

static void gen_stmt(JCC *vm, Node *node);

// Helper to resolve global variable to the canonical version in the merged program
// This is needed because AST nodes may reference Objs from original programs,
// but only the merged program's Objs have the correct offsets set by codegen
static Obj *resolve_global_var(JCC *vm, Obj *var) {
    if (!var || var->is_local) {
        return var;
    }
    
    // Look up in the merged program (vm->compiler.globals) by name
    for (Obj *g = vm->compiler.globals; g; g = g->next) {
        if (!g->is_function && strcmp(g->name, var->name) == 0) {
            return g;  // Return the canonical version from merged program
        }
    }
    
    // Fallback: return as-is (shouldn't happen if linking worked correctly)
    return var;
}

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
    // (e.g., "printf1", "sprintf2", etc.) and try the base name
    size_t len = strlen(name);
    if ((len > 1 && isdigit(name[len-1])) || 
        (len > 2 && isdigit(name[len-1]) && isdigit(name[len-2]))) {
        // Strip trailing digits to get base name
        char base_name[256];
        strncpy(base_name, name, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
        
        // Find where digits start from the end
        int base_len = len;
        while (base_len > 0 && isdigit(base_name[base_len-1])) {
            base_len--;
        }
        base_name[base_len] = '\0';
        
        // Try to find a variadic function with this base name
        for (int i = 0; i < vm->compiler.ffi_count; i++) {
            if (vm->compiler.ffi_table[i].is_variadic && strcmp(vm->compiler.ffi_table[i].name, base_name) == 0) {
                return i;
            }
        }
    }
    
    return -1;
}

// Generate code for an expression and leave result in ax
// Made non-static for K constexpr execution
void gen_expr(JCC *vm, Node *node) {
    if (!node) {
        error("codegen: null expression node");
    }

    switch (node->kind) {
        case ND_NULL_EXPR:
            // Do nothing - null expression
            return;

        case ND_NUM:
            // Load immediate value
            if (is_flonum(node->ty)) {
                // For floating-point, store the double in data segment and load it
                // Align data pointer to 8-byte boundary for doubles
                long long offset = vm->data_ptr - vm->data_seg;
                offset = (offset + 7) & ~7;
                vm->data_ptr = vm->data_seg + offset;
                
                // Store the float value
                *(double *)vm->data_ptr = node->fval;
                long long data_offset = vm->data_ptr - vm->data_seg;
                vm->data_ptr += sizeof(double);
                
                // Load address and then load the double
                emit_with_arg(vm, IMM, (long long)(vm->data_seg + data_offset));
                emit(vm, FLD);
            } else {
                emit_with_arg(vm, IMM, node->val);
            }
            return;

        case ND_VAR:
            // Load variable
            if (node->var->is_function) {
                // Function name used as value - function-to-pointer decay
                // Emit IMM with placeholder, patch later
                emit(vm, IMM);
                long long *addr_loc = ++vm->text_ptr;
                *addr_loc = 0;  // Placeholder

                // Record patch location
                if (vm->compiler.num_func_addr_patches >= MAX_CALLS) {
                    error("too many function address references");
                }
                vm->compiler.func_addr_patches[vm->compiler.num_func_addr_patches].location = addr_loc;
                vm->compiler.func_addr_patches[vm->compiler.num_func_addr_patches].function = node->var;
                vm->compiler.num_func_addr_patches++;
            } else if (node->var->is_local) {
                // Local variable or parameter - load address relative to bp
                // Parameters have positive offsets, locals have negative offsets

                // Check if variable is initialized (for uninitialized detection)
                // Only check scalar types (not arrays/structs) that will actually be loaded
                bool is_param = node->var->is_param;
                bool is_scalar = (node->ty->kind != TY_ARRAY &&
                                  node->ty->kind != TY_STRUCT &&
                                  node->ty->kind != TY_UNION);

                // Check if variable is alive (for stack instrumentation)
                if (vm->flags & JCC_STACK_INSTR) {
                    emit_with_arg(vm, CHKL, node->var->offset);
                }

                if (vm->flags & JCC_UNINIT_DETECTION && is_scalar) {
                    emit_with_arg(vm, CHKI, node->var->offset);
                }

                // Check if this is a parameter

                emit_with_arg(vm, LEA, node->var->offset);

                // For struct/union parameters (positive offset), we pass by pointer
                // So we need to load the pointer value first
                if (is_param && (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION)) {
                    // Struct/union params: load the pointer value using register-based load
                    emit_ax2r(vm, REG_T0);  // Address is in ax, move to REG_T0
                    emit_load3(vm, node->ty, REG_T1, REG_T0);  // Load pointer to REG_T1
                    emit_r2ax(vm, REG_T1);  // Result back to ax
                } else if (is_scalar) {
                    // For scalar types, load the value using register-based load
                    emit_ax2r(vm, REG_T0);  // Address is in ax, move to REG_T0
                    if (is_flonum(node->ty)) {
                        emit_load3(vm, node->ty, FREG_A0, REG_T0);  // Float to FREG_A0
                        emit_fr2fax(vm, FREG_A0);  // Result to fax
                    } else {
                        emit_load3(vm, node->ty, REG_T1, REG_T0);  // Int to REG_T1
                        emit_r2ax(vm, REG_T1);  // Result to ax
                    }
                    // Mark read access for stack instrumentation
                    if (vm->flags & JCC_STACK_INSTR) {
                        emit_with_arg(vm, MARKR, node->var->offset);
                    }
                }
            } else {
                // Global variable - resolve to canonical version from merged program
                Obj *resolved = resolve_global_var(vm, node->var);
                emit_with_arg(vm, IMM, (long long)(vm->data_seg + resolved->offset));

                // For arrays, structs, unions, and string literals, IMM gives us the address
                // For scalar types, we need to load the value using register-based load
                if (node->ty->kind != TY_ARRAY &&
                    node->ty->kind != TY_STRUCT &&
                    node->ty->kind != TY_UNION) {
                    emit_ax2r(vm, REG_T0);  // Address is in ax, move to REG_T0
                    if (is_flonum(node->ty)) {
                        emit_load3(vm, node->ty, FREG_A0, REG_T0);  // Float to FREG_A0
                        emit_fr2fax(vm, FREG_A0);  // Result to fax
                    } else {
                        emit_load3(vm, node->ty, REG_T1, REG_T0);  // Int to REG_T1
                        emit_r2ax(vm, REG_T1);  // Result to ax
                    }
                }
            }
            return;

        case ND_ASSIGN:
            // Special handling for bitfield assignment
            if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
                // Bitfield write - compute address, do read-modify-write

                // Get address of struct base first
                if (node->lhs->lhs->kind == ND_VAR) {
                    // Direct struct variable - get its address
                    if (node->lhs->lhs->var->is_local) {
                        emit_with_arg(vm, LEA, node->lhs->lhs->var->offset);
                    } else {
                        // Global struct
                        emit_with_arg(vm, IMM, (long long)(vm->data_seg + node->lhs->lhs->var->offset));
                    }
                } else {
                    // Expression evaluating to struct
                    gen_expr(vm, node->lhs->lhs);
                }

                // Add member offset using register-based ADDI3
                if (node->lhs->member->offset != 0) {
                    emit_ax2r(vm, REG_T0);  // Save struct address to temp reg
                    emit_rri(vm, ADDI3, REG_T0, REG_T0, node->lhs->member->offset);
                    emit_r2ax(vm, REG_T0);  // Result back to ax
                }

                // ax now has the member address
                emit(vm, PUSH);  // Save address

                // Evaluate the new value
                gen_expr(vm, node->rhs);
                // ax now has new value, stack has [address]

                // Perform bitfield write (handled below)
            } else {
                // Get left side address and push it
                if (node->lhs->kind == ND_VAR) {
                    if (node->lhs->var->is_local) {
                        // Check if variable is alive (for stack instrumentation)
                        if (vm->flags & JCC_STACK_INSTR) {
                            emit_with_arg(vm, CHKL, node->lhs->var->offset);
                        }
                        emit_with_arg(vm, LEA, node->lhs->var->offset);
                    } else {
                        // Global variable - resolve to canonical version
                        Obj *resolved = resolve_global_var(vm, node->lhs->var);
                        emit_with_arg(vm, IMM, (long long)(vm->data_seg + resolved->offset));
                    }
                    emit(vm, PUSH);  // Push address onto stack
                } else if (node->lhs->kind == ND_VLA_PTR) {
                    // For VLA pointer - get address of the pointer variable itself
                    // The VLA pointer is a local variable that stores the address of allocated memory
                    if (node->lhs->var->is_local) {
                        emit_with_arg(vm, LEA, node->lhs->var->offset);
                    } else {
                        error_tok(vm, node->tok, "VLA must be local");
                    }
                    emit(vm, PUSH);  // Push address onto stack
                } else if (node->lhs->kind == ND_DEREF) {
                    // For pointer dereference on left side (including array subscripts)
                    // Evaluate the address expression
                    gen_expr(vm, node->lhs->lhs);
                    emit(vm, PUSH);  // Push address onto stack
                } else if (node->lhs->kind == ND_MEMBER) {
                    // For struct member access on left side
                    // Get address of struct base first
                    if (node->lhs->lhs->kind == ND_VAR) {
                        // Direct struct variable - get its address
                        if (node->lhs->lhs->var->is_local) {
                            emit_with_arg(vm, LEA, node->lhs->lhs->var->offset);
                        } else {
                            // Global struct
                            emit_with_arg(vm, IMM, (long long)(vm->data_seg + node->lhs->lhs->var->offset));
                        }
                    } else {
                        // Expression evaluating to struct
                        gen_expr(vm, node->lhs->lhs);
                    }

                    // Add member offset using register-based ADDI3
                    if (node->lhs->member->offset != 0) {
                        emit_ax2r(vm, REG_T0);
                        emit_rri(vm, ADDI3, REG_T0, REG_T0, node->lhs->member->offset);
                        emit_r2ax(vm, REG_T0);
                    }

                    emit(vm, PUSH);  // Push member address onto stack
                } else {
                    error_tok(vm, node->tok, "invalid lvalue in assignment");
                }
            }

            // For bitfields, we already evaluated rhs above
            // For non-bitfields, evaluate right side now
            if (!(node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield)) {
                gen_expr(vm, node->rhs);
            }

            // Now ax/fax has the value/address, stack has the destination address
            if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION) {
                // For structs/unions, ax contains source address, stack has dest address
                // Use memcpy to copy the struct: memcpy(dest, src, size)
                // Stack layout: [dest]
                // ax: src
                // Need stack: [size, src, dest]
                emit(vm, PUSH);  // Push src address
                emit_with_arg(vm, IMM, node->ty->size);  // Push size
                emit(vm, PUSH);
                // Call MCPY (VM memcpy)
                emit(vm, MCPY);  // Pops size, src, dest; returns dest in ax
                // Note: We don't return the dest address for now (chibicc limitation)
            } else if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
                // Bitfield write - read-modify-write operation using registers
                // Entry: Stack: [address], ax: new_value
                // We need: address, shifted_new_value, current_value, then combine and store

                long long mask = (1LL << node->lhs->member->bit_width) - 1;
                long long clear_mask = ~(mask << node->lhs->member->bit_offset);

                // Pop address into REG_T3 (will need later for store)
                emit_pop3(vm, REG_T3);  // REG_T3 = address

                // new_value is in ax, mask it: REG_T0 = ax & mask
                emit_ax2r(vm, REG_T0);  // REG_T0 = new_value
                emit_with_arg(vm, IMM, mask);
                emit_ax2r(vm, REG_T1);  // REG_T1 = mask
                emit_rrr(vm, AND3, REG_T0, REG_T0, REG_T1);  // REG_T0 = new_value & mask

                // Shift if needed: REG_T0 = REG_T0 << bit_offset
                if (node->lhs->member->bit_offset > 0) {
                    emit_with_arg(vm, IMM, node->lhs->member->bit_offset);
                    emit_ax2r(vm, REG_T1);  // REG_T1 = bit_offset
                    emit_rrr(vm, SHL3, REG_T0, REG_T0, REG_T1);  // REG_T0 = shifted_new_value
                }

                // Load current value from address in REG_T3
                emit_load3_checked(vm, node->lhs->member->ty, REG_T2, REG_T3, 1);  // REG_T2 = current_value

                // Clear target bits: REG_T2 = REG_T2 & clear_mask
                emit_with_arg(vm, IMM, clear_mask);
                emit_ax2r(vm, REG_T1);  // REG_T1 = clear_mask
                emit_rrr(vm, AND3, REG_T2, REG_T2, REG_T1);  // REG_T2 = cleared_old_value

                // Combine: REG_T2 = REG_T2 | REG_T0 (final_value)
                emit_rrr(vm, OR3, REG_T2, REG_T2, REG_T0);

                // Store final value: *REG_T3 = REG_T2
                emit_store3(vm, node->lhs->member->ty, REG_T2, REG_T3);

                // Result in REG_T2, sync back to ax for assignment expression result
                emit_r2ax(vm, REG_T2);
            } else {
                // For scalar types, emit appropriate store instruction using registers
                // Stack has [address], ax/fax has value
                // Mark write access for stack instrumentation (before store)
                if (vm->flags & JCC_STACK_INSTR && node->lhs->kind == ND_VAR && node->lhs->var->is_local) {
                    emit_with_arg(vm, MARKW, node->lhs->var->offset);
                }
                // Pop address into REG_T1
                emit_pop3(vm, REG_T1);
                // Store value: *REG_T1 = ax (for int) or *REG_T1 = fax (for float)
                if (is_flonum(node->ty)) {
                    emit_fax2fr(vm, FREG_A0);  // Copy fax to FREG_A0
                    emit_store3(vm, node->ty, FREG_A0, REG_T1);
                } else {
                    emit_ax2r(vm, REG_T0);  // Copy ax to REG_T0
                    emit_store3(vm, node->ty, REG_T0, REG_T1);
                    emit_r2ax(vm, REG_T0);  // Preserve ax for assignment expression result
                }
            }

            // Mark local variable as initialized (for uninitialized detection)
            if (vm->flags & JCC_UNINIT_DETECTION && node->lhs->kind == ND_VAR && node->lhs->var->is_local) {
                bool is_scalar = (node->ty->kind != TY_ARRAY &&
                                  node->ty->kind != TY_STRUCT &&
                                  node->ty->kind != TY_UNION);
                if (is_scalar) {
                    emit_with_arg(vm, MARKI, node->lhs->var->offset);
                }
            }

            // Assignment expression returns the assigned value (already in ax/fax for scalars)
            // For structs, this is a known limitation
            return;

        case ND_ADD:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                // Float addition: use FPUSH/FPOP3 to safely save lhs across rhs eval
                emit(vm, FPUSH);                 // Save lhs (in fax) to stack
                gen_expr(vm, node->rhs);         // rhs result in fax
                emit_fax2fr(vm, FREG_A1);        // Save rhs to FREG_A1
                emit_fpop3(vm, FREG_A0);         // Pop lhs from stack into FREG_A0
                emit_frrr(vm, FADD3, FREG_A0, FREG_A0, FREG_A1);  // FREG_A0 = FREG_A0 + FREG_A1
                emit_fr2fax(vm, FREG_A0);        // Result back to fax for compatibility
            } else {
                // Use stack for recursion-safe saves, registers for final op
                emit(vm, PUSH);             // Save lhs to stack (safe across recursion)
                gen_expr(vm, node->rhs);    // rhs result in ax
                emit_ax2r(vm, REG_A1);      // Move rhs to REG_A1
                emit_pop3(vm, REG_A0);      // Pop lhs from stack into REG_A0
                if (vm->flags & JCC_OVERFLOW_CHECKS) {
                    // TODO: Add overflow-checked ADD3C opcode
                    emit_rrr(vm, ADD3, REG_A0, REG_A0, REG_A1);
                } else {
                    emit_rrr(vm, ADD3, REG_A0, REG_A0, REG_A1);
                }
                emit_r2ax(vm, REG_A0);      // Result back to ax for compatibility
            }

            // Check pointer arithmetic if result is a pointer type
            if (vm->flags & JCC_INVALID_ARITH && node->ty->kind == TY_PTR) {
                emit(vm, CHKPA);
            }
            return;

        case ND_SUB:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                // Float subtraction: use FPUSH/FPOP3 to safely save lhs across rhs eval
                emit(vm, FPUSH);                 // Save lhs to stack
                gen_expr(vm, node->rhs);         // rhs result in fax
                emit_fax2fr(vm, FREG_A1);        // Save rhs to FREG_A1
                emit_fpop3(vm, FREG_A0);         // Pop lhs from stack into FREG_A0
                emit_frrr(vm, FSUB3, FREG_A0, FREG_A0, FREG_A1);
                emit_fr2fax(vm, FREG_A0);
            } else {
                // Use stack for recursion-safe saves, registers for final op
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit_ax2r(vm, REG_A1);
                emit_pop3(vm, REG_A0);
                emit_rrr(vm, SUB3, REG_A0, REG_A0, REG_A1);
                emit_r2ax(vm, REG_A0);
            }

            // Check pointer arithmetic if result is a pointer type
            if (vm->flags & JCC_INVALID_ARITH && node->ty->kind == TY_PTR) {
                emit(vm, CHKPA);
            }
            return;

        case ND_MUL:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                // Float multiplication: use FPUSH/FPOP3 to safely save lhs across rhs eval
                emit(vm, FPUSH);                 // Save lhs to stack
                gen_expr(vm, node->rhs);         // rhs result in fax
                emit_fax2fr(vm, FREG_A1);        // Save rhs to FREG_A1
                emit_fpop3(vm, FREG_A0);         // Pop lhs from stack into FREG_A0
                emit_frrr(vm, FMUL3, FREG_A0, FREG_A0, FREG_A1);
                emit_fr2fax(vm, FREG_A0);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit_ax2r(vm, REG_A1);
                emit_pop3(vm, REG_A0);
                emit_rrr(vm, MUL3, REG_A0, REG_A0, REG_A1);
                emit_r2ax(vm, REG_A0);
            }
            return;

        case ND_DIV:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                // Float division: use FPUSH/FPOP3 to safely save lhs across rhs eval
                emit(vm, FPUSH);                 // Save lhs to stack
                gen_expr(vm, node->rhs);         // rhs result in fax
                emit_fax2fr(vm, FREG_A1);        // Save rhs to FREG_A1
                emit_fpop3(vm, FREG_A0);         // Pop lhs from stack into FREG_A0
                emit_frrr(vm, FDIV3, FREG_A0, FREG_A0, FREG_A1);
                emit_fr2fax(vm, FREG_A0);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit_ax2r(vm, REG_A1);
                emit_pop3(vm, REG_A0);
                emit_rrr(vm, DIV3, REG_A0, REG_A0, REG_A1);
                emit_r2ax(vm, REG_A0);
            }
            return;

        case ND_MOD:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit_ax2r(vm, REG_A1);
            emit_pop3(vm, REG_A0);
            emit_rrr(vm, MOD3, REG_A0, REG_A0, REG_A1);
            emit_r2ax(vm, REG_A0);
            return;

        case ND_EQ:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                // Float equality: use FPUSH/FPOP3 to safely save lhs across rhs eval
                emit(vm, FPUSH);                 // Save lhs to stack
                gen_expr(vm, node->rhs);         // rhs result in fax
                emit_fax2fr(vm, FREG_A1);        // Save rhs to FREG_A1
                emit_fpop3(vm, FREG_A0);         // Pop lhs from stack into FREG_A0
                emit_frrr(vm, FEQ3, REG_A0, FREG_A0, FREG_A1);  // Result in integer reg
                emit_r2ax(vm, REG_A0);           // Result to ax
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit_ax2r(vm, REG_A1);
                emit_pop3(vm, REG_A0);
                emit_rrr(vm, SEQ3, REG_A0, REG_A0, REG_A1);
                emit_r2ax(vm, REG_A0);
            }
            return;

        case ND_NE:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                // Float not-equal: use FPUSH/FPOP3 to safely save lhs across rhs eval
                emit(vm, FPUSH);                 // Save lhs to stack
                gen_expr(vm, node->rhs);         // rhs result in fax
                emit_fax2fr(vm, FREG_A1);        // Save rhs to FREG_A1
                emit_fpop3(vm, FREG_A0);         // Pop lhs from stack into FREG_A0
                emit_frrr(vm, FNE3, REG_A0, FREG_A0, FREG_A1);  // Result in integer reg
                emit_r2ax(vm, REG_A0);           // Result to ax
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit_ax2r(vm, REG_A1);
                emit_pop3(vm, REG_A0);
                emit_rrr(vm, SNE3, REG_A0, REG_A0, REG_A1);
                emit_r2ax(vm, REG_A0);
            }
            return;

        case ND_LT:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                // Float less-than: use FPUSH/FPOP3 to safely save lhs across rhs eval
                emit(vm, FPUSH);                 // Save lhs to stack
                gen_expr(vm, node->rhs);         // rhs result in fax
                emit_fax2fr(vm, FREG_A1);        // Save rhs to FREG_A1
                emit_fpop3(vm, FREG_A0);         // Pop lhs from stack into FREG_A0
                emit_frrr(vm, FLT3, REG_A0, FREG_A0, FREG_A1);  // Result in integer reg
                emit_r2ax(vm, REG_A0);           // Result to ax
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit_ax2r(vm, REG_A1);
                emit_pop3(vm, REG_A0);
                emit_rrr(vm, SLT3, REG_A0, REG_A0, REG_A1);
                emit_r2ax(vm, REG_A0);
            }
            return;

        case ND_LE:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                // Float less-equal: use FPUSH/FPOP3 to safely save lhs across rhs eval
                emit(vm, FPUSH);                 // Save lhs to stack
                gen_expr(vm, node->rhs);         // rhs result in fax
                emit_fax2fr(vm, FREG_A1);        // Save rhs to FREG_A1
                emit_fpop3(vm, FREG_A0);         // Pop lhs from stack into FREG_A0
                emit_frrr(vm, FLE3, REG_A0, FREG_A0, FREG_A1);  // Result in integer reg
                emit_r2ax(vm, REG_A0);           // Result to ax
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit_ax2r(vm, REG_A1);
                emit_pop3(vm, REG_A0);
                emit_rrr(vm, SLE3, REG_A0, REG_A0, REG_A1);
                emit_r2ax(vm, REG_A0);
            }
            return;

        case ND_BITOR:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit_ax2r(vm, REG_A1);
            emit_pop3(vm, REG_A0);
            emit_rrr(vm, OR3, REG_A0, REG_A0, REG_A1);
            emit_r2ax(vm, REG_A0);
            return;

        case ND_BITXOR:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit_ax2r(vm, REG_A1);
            emit_pop3(vm, REG_A0);
            emit_rrr(vm, XOR3, REG_A0, REG_A0, REG_A1);
            emit_r2ax(vm, REG_A0);
            return;

        case ND_BITAND:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit_ax2r(vm, REG_A1);
            emit_pop3(vm, REG_A0);
            emit_rrr(vm, AND3, REG_A0, REG_A0, REG_A1);
            emit_r2ax(vm, REG_A0);
            return;

        case ND_SHL:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit_ax2r(vm, REG_A1);
            emit_pop3(vm, REG_A0);
            emit_rrr(vm, SHL3, REG_A0, REG_A0, REG_A1);
            emit_r2ax(vm, REG_A0);
            return;

        case ND_SHR:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit_ax2r(vm, REG_A1);
            emit_pop3(vm, REG_A0);
            emit_rrr(vm, SHR3, REG_A0, REG_A0, REG_A1);
            emit_r2ax(vm, REG_A0);
            return;

        case ND_ADDR:
            // Get address of variable
            if (node->lhs->kind == ND_VAR) {
                // Check if it's a function
                if (node->lhs->var->is_function) {
                    // Function address - emit IMM with placeholder, patch later
                    emit(vm, IMM);
                    long long *addr_loc = ++vm->text_ptr;
                    *addr_loc = 0;  // Placeholder
                    
                    // Record patch location
                    if (vm->compiler.num_func_addr_patches >= MAX_CALLS) {
                        error("too many function address references");
                    }
                    vm->compiler.func_addr_patches[vm->compiler.num_func_addr_patches].location = addr_loc;
                    vm->compiler.func_addr_patches[vm->compiler.num_func_addr_patches].function = node->lhs->var;
                    vm->compiler.num_func_addr_patches++;
                } else if (node->lhs->var->is_local) {
                    emit_with_arg(vm, LEA, node->lhs->var->offset);

                    // Mark address for dangling pointer detection
                    if (vm->flags & JCC_DANGLING_DETECT || vm->flags & JCC_STACK_INSTR) {
                        // ax now contains the address of the stack variable
                        // Emit MARKA with stack offset, size, and scope_id as three operands
                        // node->ty is the pointer type, node->ty->base is what it points to
                        size_t pointed_size = node->ty->base ? node->ty->base->size : 1;
                        emit(vm, MARKA);
                        *++vm->text_ptr = node->lhs->var->offset;
                        *++vm->text_ptr = pointed_size;
                        *++vm->text_ptr = vm->current_function_scope_id;  // Scope ID
                    }

                    // Mark provenance for stack pointer
                    if (vm->flags & JCC_PROVENANCE_TRACK) {
                        size_t pointed_size = node->ty->base ? node->ty->base->size : 1;
                        emit(vm, MARKP);
                        *++vm->text_ptr = 1;  // Origin type: 1=STACK
                        *++vm->text_ptr = (long long)(vm->bp + node->lhs->var->offset);  // Base address
                        *++vm->text_ptr = pointed_size;
                    }
                } else {
                    // Global variable - resolve to canonical version
                    Obj *resolved = resolve_global_var(vm, node->lhs->var);
                    emit_with_arg(vm, IMM, (long long)(vm->data_seg + resolved->offset));

                    // Mark provenance for global pointer
                    if (vm->flags & JCC_PROVENANCE_TRACK) {
                        size_t pointed_size = node->ty->base ? node->ty->base->size : 1;
                        emit(vm, MARKP);
                        *++vm->text_ptr = 2;  // Origin type: 2=GLOBAL
                        *++vm->text_ptr = (long long)(vm->data_seg + resolved->offset);  // Base
                        *++vm->text_ptr = pointed_size;
                    }
                }
            } else if (node->lhs->kind == ND_DEREF) {
                // Address of dereferenced pointer - just evaluate the pointer
                // &*ptr is just ptr
                gen_expr(vm, node->lhs->lhs);
            } else if (node->lhs->kind == ND_COMMA) {
                // Compound literal: ND_COMMA(init, var)
                // Execute the initialization first (lhs)
                gen_expr(vm, node->lhs->lhs);
                // Then get the address of the variable (rhs)
                // The rhs should be an ND_VAR node
                if (node->lhs->rhs->kind == ND_VAR) {
                    if (node->lhs->rhs->var->is_local) {
                        emit_with_arg(vm, LEA, node->lhs->rhs->var->offset);
                    } else {
                        emit_with_arg(vm, IMM, (long long)(vm->data_seg + node->lhs->rhs->var->offset));
                    }
                } else {
                    error_tok(vm, node->tok, "invalid compound literal in address-of operator");
                }
            } else if (node->lhs->kind == ND_MEMBER) {
                // Address of struct member: &(struct.member)
                // Get address of struct base first
                if (node->lhs->lhs->kind == ND_VAR) {
                    // Direct struct variable - get its address
                    if (node->lhs->lhs->var->is_local) {
                        emit_with_arg(vm, LEA, node->lhs->lhs->var->offset);
                    } else {
                        // Global struct
                        emit_with_arg(vm, IMM, (long long)(vm->data_seg + node->lhs->lhs->var->offset));
                    }
                } else {
                    // Expression evaluating to struct
                    gen_expr(vm, node->lhs->lhs);
                }
                
                // Add member offset using register-based ADDI3
                if (node->lhs->member->offset != 0) {
                    emit_ax2r(vm, REG_T0);
                    emit_rri(vm, ADDI3, REG_T0, REG_T0, node->lhs->member->offset);
                    emit_r2ax(vm, REG_T0);
                }
            } else {
                error_tok(vm, node->tok, "invalid operand for address-of operator");
            }
            return;

        case ND_DEREF:
            // Dereference pointer
            gen_expr(vm, node->lhs);
            // For structs, unions, and arrays, just evaluate the pointer address
            // (don't try to load the value since they're aggregate types)
            if (node->ty->kind == TY_STRUCT ||
                node->ty->kind == TY_UNION ||
                node->ty->kind == TY_ARRAY) {
                // Address is already in ax, nothing more to do
                return;
            }
            // For scalar types, load the value from the address using register-based LDR
            // Use REG_T0 for address and REG_T1 for value to avoid conflicting
            // with argument registers REG_A0-A7 during function calls
            emit_ax2r(vm, REG_T0);  // Move pointer address to REG_T0
            if (is_flonum(node->ty)) {
                emit_load3(vm, node->ty, FREG_A0, REG_T0);  // Load float into FREG_A0
                emit_fr2fax(vm, FREG_A0);  // Move result to fax
            } else {
                emit_load3(vm, node->ty, REG_T1, REG_T0);  // Use REG_T1, not REG_A0
                emit_r2ax(vm, REG_T1);  // Move result to ax
            }
            return;

        case ND_NEG:
            // Unary minus
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                // Float negation: use 3-register float op
                emit_fax2fr(vm, FREG_A0);        // Save operand to FREG_A0
                emit_frr(vm, FNEG3, FREG_A0, FREG_A0);  // FREG_A0 = -FREG_A0
                emit_fr2fax(vm, FREG_A0);        // Result back to fax
            } else {
                // Integer negation: use 3-register op
                emit_ax2r(vm, REG_A0);           // Move operand to REG_A0
                emit_rr(vm, NEG3, REG_A0, REG_A0);  // REG_A0 = -REG_A0
                emit_r2ax(vm, REG_A0);           // Result back to ax
            }
            return;

        case ND_CAST: {
            // Type cast - handle all conversions with proper sign/zero extension
            gen_expr(vm, node->lhs);
            
            Type *from = node->lhs->ty;
            Type *to = node->ty;
            
            // Special case: Array-to-pointer decay (common for string literals)
            // Arrays decay to pointers at the value level (no runtime conversion needed)
            // The address is already in ax from the array generation
            if (from->kind == TY_ARRAY && to->kind == TY_PTR) {
                // No conversion needed - array address is already a pointer
                return;
            }
            
            // Handle float <-> integer conversions
            bool from_float = is_flonum(from);
            bool to_float = is_flonum(to);
            
            if (from_float && !to_float) {
                // Float to int conversion
                emit(vm, F2I);
                from = ty_long;  // F2I produces long long
                from_float = false;
            } else if (!from_float && to_float) {
                // Int to float conversion
                emit(vm, I2F);
                // Result is in fax, no further conversion needed
                return;
            } else if (from_float && to_float) {
                // Float to float conversion (double <-> float)
                // VM uses double for all floats, no conversion needed
                return;
            }
            
            // Now handle integer-to-integer conversions
            // Strategy: Only emit conversion if sizes are DIFFERENT or signedness differs
            // If same size and signedness, no conversion needed (avoids double-extension)
            
            if (from->size == to->size && from->is_unsigned == to->is_unsigned) {
                // No conversion needed - same type
                return;
            }
            
            if (to->size < from->size) {
                // Truncating to smaller type - truncate and extend
                if (to->size == 1) {
                    emit(vm, to->is_unsigned ? ZX1 : SX1);
                } else if (to->size == 2) {
                    emit(vm, to->is_unsigned ? ZX2 : SX2);
                } else if (to->size == 4) {
                    emit(vm, to->is_unsigned ? ZX4 : SX4);
                }
            } else if (to->size > from->size) {
                // Extending to larger type
                if (from->size == 1) {
                    emit(vm, from->is_unsigned ? ZX1 : SX1);
                } else if (from->size == 2) {
                    emit(vm, from->is_unsigned ? ZX2 : SX2);
                } else if (from->size == 4) {
                    emit(vm, from->is_unsigned ? ZX4 : SX4);
                }
            } else {
                // Same size but different signedness
                if (to->size == 1) {
                    emit(vm, to->is_unsigned ? ZX1 : SX1);
                } else if (to->size == 2) {
                    emit(vm, to->is_unsigned ? ZX2 : SX2);
                } else if (to->size == 4) {
                    emit(vm, to->is_unsigned ? ZX4 : SX4);
                }
            }
            
            return;
        }

        case ND_COMMA:
            // Comma operator: evaluate left, discard, evaluate right, return right
            gen_expr(vm, node->lhs);
            gen_expr(vm, node->rhs);
            return;

        case ND_COND: {
            // Ternary operator: cond ? then : els
            // Evaluate condition
            gen_expr(vm, node->cond);
            
            // Jump to else branch if condition is false (zero)
            emit(vm, JZ);
            long long *jz_addr = ++vm->text_ptr;
            *jz_addr = 0;  // Write placeholder
            
            // Generate then expression
            gen_expr(vm, node->then);
            
            // Jump over else branch
            emit(vm, JMP);
            long long *jmp_addr = ++vm->text_ptr;
            *jmp_addr = 0;  // Write placeholder
            
            // Patch JZ to jump here (else branch)
            *jz_addr = (long long)(vm->text_ptr + 1);
            
            // Generate else expression
            gen_expr(vm, node->els);
            
            // Patch JMP to jump here (after ternary)
            *jmp_addr = (long long)(vm->text_ptr + 1);
            
            // Result (from either branch) is in ax
            return;
        }

        case ND_MEMZERO:
            // Zero-initialize a local variable
            // For now, we'll skip this optimization and let assignments handle it
            // In a more complete implementation, we'd emit code to zero the memory
            return;

        case ND_NOT:
            // Logical NOT: !expr
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            emit_with_arg(vm, IMM, 0);
            emit(vm, EQ);  // Compare with 0
            return;

        case ND_BITNOT:
            // Bitwise NOT: ~expr
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            emit_with_arg(vm, IMM, -1);
            emit(vm, XOR);  // XOR with all 1s
            return;

        case ND_LOGAND:
            // Logical AND with short-circuit: left && right
            gen_expr(vm, node->lhs);
            emit(vm, JZ);
            long long *jz_and = ++vm->text_ptr;
            *jz_and = 0;  // Write placeholder
            
            gen_expr(vm, node->rhs);
            emit(vm, JZ);
            long long *jz_and2 = ++vm->text_ptr;
            *jz_and2 = 0;  // Write placeholder
            
            // Both true
            emit_with_arg(vm, IMM, 1);
            emit(vm, JMP);
            long long *jmp_and = ++vm->text_ptr;
            *jmp_and = 0;  // Write placeholder
            
            // At least one false
            long long target = (long long)(vm->text_ptr + 1);
            *jz_and = target;
            *jz_and2 = target;
            emit_with_arg(vm, IMM, 0);
            *jmp_and = (long long)(vm->text_ptr + 1);
            return;

        case ND_LOGOR:
            // Logical OR with short-circuit: left || right
            gen_expr(vm, node->lhs);
            emit(vm, JNZ);
            long long *jnz_or = ++vm->text_ptr;
            *jnz_or = 0;  // Write placeholder
            
            gen_expr(vm, node->rhs);
            emit(vm, JNZ);
            long long *jnz_or2 = ++vm->text_ptr;
            *jnz_or2 = 0;  // Write placeholder
            
            // Both false
            emit_with_arg(vm, IMM, 0);
            emit(vm, JMP);
            long long *jmp_or = ++vm->text_ptr;
            *jmp_or = 0;  // Write placeholder
            
            // At least one true
            *jnz_or = (long long)(vm->text_ptr + 1);
            *jnz_or2 = (long long)(vm->text_ptr + 1);
            emit_with_arg(vm, IMM, 1);
            *jmp_or = (long long)(vm->text_ptr + 1);
            return;

        case ND_FUNCALL: {
            // Check if this is a builtin alloca call
            if (node->lhs->kind == ND_VAR && node->lhs->var == vm->compiler.builtin_alloca) {
                // Special handling for alloca: it's a VM instruction, not a function call
                // Evaluate the size argument into ax
                if (!node->args) {
                    error_tok(vm, node->tok, "alloca requires a size argument");
                }
                gen_expr(vm, node->args);
                // ax now contains the size to allocate
                // Call MALC (VM malloc)
                emit(vm, PUSH);  // Push size onto stack
                emit(vm, MALC);  // Pops size, returns pointer in ax
                // Result (pointer to allocated memory) is in ax
                return;
            }

            // Check if this is setjmp or longjmp - these are VM builtins
            if (node->lhs->kind == ND_VAR && node->lhs->var == vm->compiler.builtin_setjmp) {
                // setjmp(jmp_buf env) - save execution context
                if (!node->args) {
                    error_tok(vm, node->tok, "setjmp requires a jmp_buf argument");
                }
                // Evaluate jmp_buf address (it's an array, so decays to pointer)
                gen_expr(vm, node->args);
                // Don't push - SETJMP reads from ax directly
                emit(vm, SETJMP);  // Save context, returns 0 in ax
                // Result (0 or longjmp value) is in ax
                return;
            }

            if (node->lhs->kind == ND_VAR && node->lhs->var == vm->compiler.builtin_longjmp) {
                // longjmp(jmp_buf env, int val) - restore execution context
                // Arguments: env, val
                if (!node->args || !node->args->next) {
                    error_tok(vm, node->tok, "longjmp requires jmp_buf and int arguments");
                }
                // Push arguments right-to-left: val, then env
                gen_expr(vm, node->args->next);  // val
                emit(vm, PUSH);
                gen_expr(vm, node->args);        // env (jmp_buf address)
                emit(vm, PUSH);
                emit(vm, LONGJMP);  // Restore context and jump
                // This instruction does not return normally
                return;
            }

            // Check if this is malloc/free and memory safety is enabled
            // When safety features are active, use VM heap (MALC/MFRE) instead of FFI
            int use_vm_heap = (vm->flags & JCC_VM_HEAP_TRIGGERS) != 0;

            if (use_vm_heap && node->lhs->kind == ND_VAR && node->lhs->var->name) {
                if (strcmp(node->lhs->var->name, "malloc") == 0) {
                    // Emit VM malloc (MALC) instead of FFI call
                    if (!node->args) {
                        error_tok(vm, node->tok, "malloc requires a size argument");
                    }
                    gen_expr(vm, node->args);  // size in ax
                    emit(vm, PUSH);            // Push size onto stack
                    emit(vm, MALC);            // Allocate, returns pointer in ax
                    return;
                }

                if (strcmp(node->lhs->var->name, "free") == 0) {
                    // Emit VM free (MFRE) instead of FFI call
                    if (!node->args) {
                        error_tok(vm, node->tok, "free requires a pointer argument");
                    }
                    gen_expr(vm, node->args);  // pointer in ax
                    emit(vm, PUSH);            // Push pointer onto stack
                    emit(vm, MFRE);            // Free the allocation
                    // ax contains 0 after MFRE
                    return;
                }

                if (strcmp(node->lhs->var->name, "realloc") == 0) {
                    // Emit VM realloc (REALC) instead of FFI call
                    // realloc(ptr, size)
                    if (!node->args || !node->args->next) {
                        error_tok(vm, node->tok, "realloc requires two arguments (ptr, size)");
                    }
                    gen_expr(vm, node->args);        // ptr in ax
                    emit(vm, PUSH);                  // Push ptr onto stack
                    gen_expr(vm, node->args->next);  // size in ax
                    emit(vm, PUSH);                  // Push size onto stack
                    emit(vm, REALC);                 // Realloc, returns new pointer in ax
                    return;
                }

                if (strcmp(node->lhs->var->name, "calloc") == 0) {
                    // Emit VM calloc (CALC) instead of FFI call
                    // calloc(count, size)
                    if (!node->args || !node->args->next) {
                        error_tok(vm, node->tok, "calloc requires two arguments (count, size)");
                    }
                    gen_expr(vm, node->args);        // count in ax
                    emit(vm, PUSH);                  // Push count onto stack
                    gen_expr(vm, node->args->next);  // size in ax
                    emit(vm, PUSH);                  // Push size onto stack
                    emit(vm, CALC);                  // Calloc, returns zeroed pointer in ax
                    return;
                }
            }

            // Function call: store arguments in registers REG_A0-REG_A7, then CALL
            // Struct/union returns handled by copy-before-return in callee
            
            // Count arguments
            int nargs = 0;
            for (Node *arg = node->args; arg; arg = arg->next) {
                nargs++;
            }
            
            // Count fixed parameters (for variadic function handling)
            // Varargs are passed in int registers even if they are floats
            int fixed_param_count = 0;
            bool is_variadic_call = node->func_ty && node->func_ty->is_variadic;
            if (is_variadic_call) {
                for (Type *p = node->func_ty->params; p; p = p->next) {
                    fixed_param_count++;
                }
            }
            
            // Check how many args - first 8 go in registers, rest on stack
            // Stack args are pushed RIGHT-TO-LEFT before register args are set up
            int num_stack_args = (nargs > 8) ? (nargs - 8) : 0;
            
            // Early detection of FFI call - for FFI calls we use direct stack-based
            // arg passing instead of register-based to avoid clobbering issues
            bool is_ffi_call = false;
            int ffi_idx = -1;
            if (node->lhs->kind == ND_VAR && node->lhs->var->is_function) {
                ffi_idx = find_ffi_function(vm, node->lhs->var->name);
                is_ffi_call = (ffi_idx >= 0);
            }
            
            // For FFI calls, push ALL args directly to stack in reverse order
            // This avoids register clobbering issues with expressions
            if (is_ffi_call) {
                // Collect args in array
                Node **arg_array = calloc(nargs, sizeof(Node*));
                if (!arg_array) error("out of memory");
                int idx = 0;
                for (Node *a = node->args; a; a = a->next) {
                    arg_array[idx++] = a;
                }
                
                // Push all args in reverse order (right-to-left)
                for (int j = nargs - 1; j >= 0; j--) {
                    gen_expr(vm, arg_array[j]);
                    if (is_flonum(arg_array[j]->ty)) {
                        emit(vm, FPUSH);  // Float result in fax
                    } else {
                        emit(vm, PUSH);   // Int result in ax
                    }
                }
                
                // Compute double_arg_mask based on actual arguments
                uint64_t double_arg_mask = 0;
                int arg_mask_idx = 0;
                for (Node *arg = node->args; arg && arg_mask_idx < 64; arg = arg->next, arg_mask_idx++) {
                    if (is_flonum(arg->ty)) {
                        double_arg_mask |= (1ULL << arg_mask_idx);
                    }
                }
                
                // Push double_arg_mask, arg count, then function index
                emit_with_arg(vm, IMM, double_arg_mask);
                emit(vm, PUSH);                  // Save double_arg_mask on stack
                emit_with_arg(vm, IMM, nargs);   // Actual argument count
                emit(vm, PUSH);                  // Save on stack
                emit_with_arg(vm, IMM, ffi_idx); // Function index in ax
                emit(vm, CALLF);
                
                free(arg_array);
                // Result is already in ax/fax
                return;
            }
            
            // Non-FFI call: use register-based passing
            // If we have stack args (args 9+), push them first in reverse order
            if (num_stack_args > 0) {
                // Collect args in array for stack arg handling
                Node **arg_array = calloc(nargs, sizeof(Node*));
                if (!arg_array) error("out of memory");
                int idx = 0;
                for (Node *a = node->args; a; a = a->next) {
                    arg_array[idx++] = a;
                }
                
                // Push stack args (args 8, 9, ... nargs-1) in reverse order
                for (int j = nargs - 1; j >= 8; j--) {
                    gen_expr(vm, arg_array[j]);
                    emit(vm, PUSH);
                }
                free(arg_array);
            }
            
            // Evaluate first 8 arguments left-to-right and store in registers
            // REG_A0 = first int arg, REG_A1 = second int arg, etc.
            // FREG_A0 = first float arg, FREG_A1 = second float arg, etc.
            // Args 9+ were already pushed to stack above.
            // 
            // CRITICAL: When an argument is itself a function call, that call
            // will clobber REG_A0-REG_A7 and FREG_A0-FREG_A7. We must save any
            // already-prepared arguments before evaluating nested function calls.
            int int_arg_idx = 0;
            int float_arg_idx = 0;
            int total_reg_args = 0;
            int num_reg_args = (nargs > 8) ? 8 : nargs;
            for (Node *arg = node->args; arg && total_reg_args < num_reg_args; arg = arg->next) {
                // Check if this arg expression contains a function call
                // If so, we need to save all previously-evaluated arg registers
                bool needs_save = false;
                if (int_arg_idx > 0 || float_arg_idx > 0) {
                    // Check if arg is or contains a function call
                    Node *check = arg;
                    // Simple check: direct function call as argument
                    if (check->kind == ND_FUNCALL) {
                        needs_save = true;
                    }
                    // Also check for binary ops where either side could be a call
                    if (check->lhs && check->lhs->kind == ND_FUNCALL) needs_save = true;
                    if (check->rhs && check->rhs->kind == ND_FUNCALL) needs_save = true;
                }
                
                // Save previously-prepared registers if needed
                // For now, save int registers (float registers saved via FPUSH would be complex)
                int saved_int_args = 0;  // Track how many we saved
                if (needs_save) {
                    saved_int_args = int_arg_idx;  // Save count BEFORE gen_expr
                    // Push all already-prepared int arg registers to stack (reverse order)
                    for (int j = int_arg_idx - 1; j >= 0; j--) {
                        emit_r2ax(vm, REG_A0 + j);
                        emit(vm, PUSH);
                    }
                    // TODO: Save float registers via FPUSH similarly
                }
                
                // Special handling for struct/union arguments that are function call results
                // We need to copy them to temporaries to avoid return buffer reuse issues
                if (arg->kind == ND_FUNCALL && arg->ty &&
                    (arg->ty->kind == TY_STRUCT || arg->ty->kind == TY_UNION)) {

                    int struct_size = arg->ty->size;

                    // Step 1: Allocate temporary heap space
                    emit_with_arg(vm, IMM, struct_size);
                    emit(vm, PUSH);
                    emit(vm, MALC);  // ax = temp_addr

                    // Step 2: Save temp address and evaluate function call
                    emit(vm, PUSH);  // stack = [temp_addr]
                    gen_expr(vm, arg);  // ax = return_buffer_addr, stack = [temp_addr]

                    // Step 3: Build stack for MCPY: need [size, src, dest]
                    emit(vm, PUSH);  // stack = [temp_addr, return_buffer_addr]

                    // Push size
                    emit_with_arg(vm, IMM, struct_size);
                    emit(vm, PUSH);  // stack = [temp_addr, return_buffer_addr, size]

                    // Step 4: Copy from return buffer to temp
                    emit(vm, MCPY);  // Returns temp_addr in ax

                } else {
                    // For non-struct-returning calls and other expressions
                    gen_expr(vm, arg);
                }

                // Store result in appropriate register based on type
                // For variadic calls, varargs (args beyond fixed count) must go in int regs
                bool is_vararg = is_variadic_call && (total_reg_args >= fixed_param_count);
                
                if (is_flonum(arg->ty)) {
                    // Float/double argument - result is in fax after gen_expr
                    if (is_vararg) {
                        // Vararg float: move bits to int register
                        // ENT3 will spill int regs to stack; va_arg reads from stack
                        if (int_arg_idx < 8) {
                            emit_fax2r(vm, REG_A0 + int_arg_idx);
                            int_arg_idx++;
                        }
                    } else {
                        // Fixed param float: store in float register
                        if (float_arg_idx < 8) {
                            emit_fax2fr(vm, FREG_A0 + float_arg_idx);
                            float_arg_idx++;
                        }
                    }
                } else {
                    // Integer/pointer argument - result is in ax after gen_expr
                    // Store in integer register file
                    if (int_arg_idx < 8) {
                        emit_ax2r(vm, REG_A0 + int_arg_idx);
                        int_arg_idx++;
                    }
                }
                
                // Restore previously-saved registers if needed
                if (needs_save) {
                    // Pop back in forward order (use saved count, not current int_arg_idx)
                    for (int j = 0; j < saved_int_args; j++) {
                        emit_pop3(vm, REG_A0 + j);
                    }
                }
                
                total_reg_args++;
            }
            
            // Determine if this is a direct or indirect function call
            // Direct call: function name is a ND_VAR with is_function=true
            // Indirect call: anything else (function pointer variable, dereferenced pointer, etc.)
            
            // Note: FFI calls already handled above via early return
            
            if (node->lhs->kind == ND_VAR && node->lhs->var->is_function) {
                // Regular VM function - use CALL with static address
                Obj *fn = node->lhs->var;
                emit(vm, CALL);
                long long *call_addr = ++vm->text_ptr;
                
                // Store patch information
                if (vm->compiler.num_call_patches >= MAX_CALLS) {
                    error("too many function calls");
                }
                vm->compiler.call_patches[vm->compiler.num_call_patches].location = call_addr;
                vm->compiler.call_patches[vm->compiler.num_call_patches].function = fn;
                vm->compiler.num_call_patches++;
                
                // Emit placeholder (will be patched)
                *call_addr = 0;
            } else {
                // Indirect function call through pointer - use CALLI
                // Evaluate the function expression to get the address
                gen_expr(vm, node->lhs);
                // ax now contains the function address
                emit(vm, CALLI);
            }
            
            // Read return value from REG_A0 or FREG_A0 back into ax/fax
            // (FFI calls already returned above)
            Type *ret_type = node->ty;
            if (ret_type && is_flonum(ret_type)) {
                // Float return: result is in fregs[FREG_A0], copy to fax
                emit_fr2fax(vm, FREG_A0);
            } else {
                // Integer/pointer return: result is in regs[REG_A0], copy to ax
                emit_r2ax(vm, REG_A0);
            }
            
            // Result is in ax (int) or fax (float)
            // For struct/union returns, ax contains the address of the struct
            return;
        }

        case ND_MEMBER:
            // Struct member access: struct.member or ptr->member
            // The lhs is the struct expression, member field has the offset
            // We need to get the address of the struct base first

            // Just evaluate the lhs - it will give us the struct address
            // ND_VAR handles both local structs and parameters correctly
            if (node->lhs->kind == ND_DEREF) {
                // Pointer dereference: ptr->member
                // We need the address the pointer points to, not the dereferenced value
                // So just evaluate the pointer itself (lhs of DEREF)
                gen_expr(vm, node->lhs->lhs);
            } else {
                // For all other cases (including ND_VAR), use gen_expr
                // This ensures struct parameters are handled correctly
                gen_expr(vm, node->lhs);
            }

            // ax now contains the address of the struct
            // Add member offset using register-based ADDI3
            if (node->member->offset != 0) {
                emit_ax2r(vm, REG_T0);
                emit_rri(vm, ADDI3, REG_T0, REG_T0, node->member->offset);
                emit_r2ax(vm, REG_T0);
            }

            // Now ax contains the address of the member
            // Handle bitfield reads specially
            if (node->member->is_bitfield) {
                // Load the underlying storage unit using register-based load
                // Address is in ax (synced earlier via emit_r2ax if offset was added)
                emit_ax2r(vm, REG_T0);  // Address to REG_T0
                emit_load3_checked(vm, node->member->ty, REG_T1, REG_T0, 1);  // Load to REG_T1

                // Extract bits: (value >> bit_offset) & ((1 << bit_width) - 1)
                if (node->member->bit_offset > 0) {
                    // REG_T1 = REG_T1 >> bit_offset
                    emit_with_arg(vm, IMM, node->member->bit_offset);
                    emit_ax2r(vm, REG_T2);  // bit_offset in REG_T2
                    emit_rrr(vm, SHR3, REG_T1, REG_T1, REG_T2);
                }

                // Create mask and apply it: REG_T1 = REG_T1 & mask
                long long mask = (1LL << node->member->bit_width) - 1;
                emit_with_arg(vm, IMM, mask);
                emit_ax2r(vm, REG_T2);  // mask in REG_T2
                emit_rrr(vm, AND3, REG_T1, REG_T1, REG_T2);

                // Sign extend if signed type
                if (!node->ty->is_unsigned) {
                    // Check if sign bit is set
                    long long sign_bit = 1LL << (node->member->bit_width - 1);
                    emit_with_arg(vm, IMM, sign_bit);
                    emit_ax2r(vm, REG_T2);  // sign_bit in REG_T2
                    // REG_T0 = REG_T1 & sign_bit (test sign bit)
                    emit_rrr(vm, AND3, REG_T0, REG_T1, REG_T2);

                    // If sign bit is 0, skip sign extension
                    emit_r2ax(vm, REG_T0);  // ax = sign test result
                    emit(vm, JZ);
                    long long *jz_addr = ++vm->text_ptr;
                    *jz_addr = 0;  // Placeholder

                    // Negative path: Sign extend - REG_T1 = REG_T1 | ~mask
                    emit_with_arg(vm, IMM, ~mask);
                    emit_ax2r(vm, REG_T2);  // ~mask in REG_T2
                    emit_rrr(vm, OR3, REG_T1, REG_T1, REG_T2);

                    // Patch jump target
                    *jz_addr = (long long)(vm->text_ptr + 1);
                }

                // Result in REG_T1, sync back to ax
                emit_r2ax(vm, REG_T1);
            } else {
                // Non-bitfield member: load the value unless it's an array or nested struct
                if (node->ty->kind != TY_ARRAY && node->ty->kind != TY_STRUCT && node->ty->kind != TY_UNION) {
                    // Member access: load value using register-based LDR
                    // Use REG_T0 for address and REG_T1 for loaded value to avoid
                    // conflicting with argument registers REG_A0-A7 during function calls
                    emit_ax2r(vm, REG_T0);  // Address is in ax, move to REG_T0
                    if (is_flonum(node->ty)) {
                        emit_load3(vm, node->ty, FREG_A0, REG_T0);  // Float regs don't conflict
                        emit_fr2fax(vm, FREG_A0);
                    } else {
                        emit_load3(vm, node->ty, REG_T1, REG_T0);  // Use REG_T1, not REG_A0
                        emit_r2ax(vm, REG_T1);
                    }
                }
            }
            return;

        case ND_STMT_EXPR:
            // Statement expression: ({ stmt1; stmt2; expr; })
            // Execute all statements in the body, last expression leaves result in ax
            for (Node *n = node->body; n; n = n->next) {
                gen_stmt(vm, n);
            }
            return;

        case ND_VLA_PTR:
            // VLA pointer/designator: load the stored pointer value
            // VLAs are implemented by storing a pointer to dynamically allocated memory
            // The pointer itself is a local variable
            if (node->var->is_local) {
                emit_with_arg(vm, LEA, node->var->offset);
                emit(vm, LI);  // Load the pointer value
            } else {
                error_tok(vm, node->tok, "VLA must be local");
            }
            return;

        case ND_LABEL_VAL:
            // Label address: &&label
            // Emit IMM with label address (will be patched later)
            emit(vm, IMM);
            long long *label_addr_loc = ++vm->text_ptr;

            // Record patch information (similar to goto patches)
            if (vm->compiler.num_goto_patches >= MAX_LABELS) {
                error_tok(vm, node->tok, "too many label references");
            }
            vm->compiler.goto_patches[vm->compiler.num_goto_patches].name = node->label;
            vm->compiler.goto_patches[vm->compiler.num_goto_patches].unique_label = node->unique_label;
            vm->compiler.goto_patches[vm->compiler.num_goto_patches].location = label_addr_loc;
            vm->compiler.num_goto_patches++;

            *label_addr_loc = 0;  // Placeholder
            return;

        default:
            error_tok(vm, node->tok, "codegen: unsupported expression node kind %d", node->kind);
    }
}

static void make_label(JCC *vm, Node *node, char *label_name, char *unique_label) {
    if (vm->compiler.num_labels >= MAX_LABELS)
        error_tok(vm, node->tok, "too many labels");
    vm->compiler.label_table[vm->compiler.num_labels].name = label_name;
    vm->compiler.label_table[vm->compiler.num_labels].unique_label = unique_label;
    vm->compiler.label_table[vm->compiler.num_labels].address = vm->text_ptr + 1;
    vm->compiler.num_labels++;
}

// Helper to emit VLA cleanup code  
// Must preserve ax (return value) during cleanup
static void emit_vla_cleanup(JCC *vm) {
    Obj *fn = vm->compiler.current_codegen_fn;
    if (!fn) return;
    
    // Check if there are any VLAs to clean up
    bool has_vlas = false;
    for (Obj *var = fn->locals; var; var = var->next) {
        if (var->ty->kind == TY_VLA && var->is_local) {
            has_vlas = true;
            break;
        }
    }
    
    if (!has_vlas) return;
    
    // Save return value (ax) on stack
    emit(vm, PUSH);
    
    // Free all VLA allocations in reverse order
    for (Obj *var = fn->locals; var; var = var->next) {
        if (var->ty->kind == TY_VLA && var->is_local) {
            // Load VLA pointer address
            emit_with_arg(vm, LEA, var->offset);
            emit(vm, LI);  // Load the pointer value
            emit(vm, PUSH);  // Push pointer onto stack for free
            // Call MFRE (VM free)
            emit(vm, MFRE);  // Pops pointer, sets ax=0
        }
    }
    
    // Restore return value: pop saved value into ax
    // The saved return value is at top of stack
    // We use: IMM 0; ADD which does ax = *sp++ + 0, effectively popping into ax
    emit_with_arg(vm, IMM, 0);
    emit(vm, ADD);  // ax = *sp++ + ax = saved_value + 0
}

// Check if a switch statement is dense enough to benefit from a jump table
// A switch is "dense" if the jump table would be space-efficient
static bool is_dense_switch(Node *node, long *out_min, long *out_max, int *out_count) {
    if (!node->case_next) {
        return false;
    }

    long min = LLONG_MAX;
    long max = LLONG_MIN;
    int count = 0;

    // Find min, max, and count of all case values
    for (Node *c = node->case_next; c; c = c->case_next) {
        for (long val = c->begin; val <= c->end; val++) {
            if (val < min) min = val;
            if (val > max) max = val;
            count++;
        }
    }

    if (out_min) *out_min = min;
    if (out_max) *out_max = max;
    if (out_count) *out_count = count;

    // Dense if table size is reasonable and density >= 40%
    long table_size = max - min + 1;

    return table_size <= 1024 &&           // Don't create huge tables
           count >= 3 &&                   // Need at least 3 cases to benefit
           (count * 100 / table_size) >= 40;  // At least 40% density
}

// Generate optimized dense switch using jump table
static void gen_dense_switch(JCC *vm, Node *node, long min_case, long max_case) {
    long table_size = max_case - min_case + 1;

    // 1. Evaluate condition ONCE
    gen_expr(vm, node->cond);
    // ax now contains condition value

    // 2. Normalize: ax = ax - min_case
    if (min_case != 0) {
        emit(vm, PUSH);              // Save condition on stack
        emit_with_arg(vm, IMM, min_case);
        emit(vm, SUB);               // ax = condition - min_case
    }
    // ax now contains normalized index (0-based)

    // 3. Bounds check: if (ax < 0 || ax >= table_size) goto default
    // We'll duplicate the normalized index for each check

    // Check if normalized_index < 0
    emit(vm, PUSH);  // Stack: [normalized_index]
    emit_with_arg(vm, IMM, 0);
    emit(vm, LT);  // ax = (normalized_index < 0), Stack: []
    emit(vm, JNZ);
    long long *below_zero_jump = ++vm->text_ptr;

    // Re-evaluate and normalize to get index back
    gen_expr(vm, node->cond);
    if (min_case != 0) {
        emit(vm, PUSH);
        emit_with_arg(vm, IMM, min_case);
        emit(vm, SUB);
    }

    // Check if normalized_index >= table_size
    emit(vm, PUSH);  // Stack: [normalized_index]
    emit_with_arg(vm, IMM, table_size);
    emit(vm, GE);  // ax = (normalized_index >= table_size), Stack: []
    emit(vm, JNZ);
    long long *above_max_jump = ++vm->text_ptr;

    // Re-evaluate and normalize one more time for JMPT
    gen_expr(vm, node->cond);
    if (min_case != 0) {
        emit(vm, PUSH);
        emit_with_arg(vm, IMM, min_case);
        emit(vm, SUB);
    }

    // 4. Table lookup: use JMPT with index in ax
    emit(vm, JMPT);
    long long *table_addr_slot = ++vm->text_ptr;  // Reserve slot for table address

    // 5. Emit jump table (reserve space)
    long long *jump_table_start = vm->text_ptr + 1;
    *table_addr_slot = (long long)jump_table_start;

    // Allocate and initialize jump table
    // Initialize all entries to point to default case (will be patched later)
    long long *jump_table = malloc(table_size * sizeof(long long));
    if (!jump_table) {
        error_tok(vm, node->tok, "failed to allocate jump table");
    }

    // Initialize all entries to ~0ULL (sentinel for unfilled entries)
    for (long i = 0; i < table_size; i++) {
        jump_table[i] = ~0ULL;
    }

    // Emit placeholders in text segment
    for (long i = 0; i < table_size; i++) {
        emit(vm, 0);  // Placeholder, will patch later
    }

    // 6. Set up tracking for case label positions
    // Store the jump table pointer in VM so ND_CASE can fill it during body generation
    long long *old_switch_table = vm->compiler.current_switch_table;
    long old_switch_min = vm->compiler.current_switch_min;
    long old_switch_size = vm->compiler.current_switch_size;
    Node *old_switch_default = vm->compiler.current_switch_default;
    vm->compiler.current_switch_table = jump_table;
    vm->compiler.current_switch_min = min_case;
    vm->compiler.current_switch_size = table_size;
    vm->compiler.current_switch_default = node->default_case;

    // Generate the entire switch body
    // Case labels will fill in their positions in the jump table as they're encountered
    gen_stmt(vm, node->then);

    // Restore previous switch context
    vm->compiler.current_switch_table = old_switch_table;
    vm->compiler.current_switch_min = old_switch_min;
    vm->compiler.current_switch_size = old_switch_size;
    vm->compiler.current_switch_default = old_switch_default;

    // 7. Default case (bounds check failed or holes in table)
    long long *default_start = vm->text_ptr + 1;
    *below_zero_jump = (long long)default_start;
    *above_max_jump = (long long)default_start;

    // Fill any unfilled table entries with default address
    for (long i = 0; i < table_size; i++) {
        if (jump_table[i] == ~0ULL) {
            jump_table[i] = (long long)default_start;
        }
    }

    // 8. Patch jump table in text segment
    memcpy(jump_table_start, jump_table, table_size * sizeof(long long));
    free(jump_table);

    if (node->default_case) {
        gen_stmt(vm, node->default_case->lhs);
    }
    // Falls through to break label
}

// Generate sparse switch using linear search (original implementation)
// For sparse switches, the overhead of saving/loading isn't worth it for few cases
static void gen_sparse_switch(JCC *vm, Node *node) {
    #define MAX_CASES 256
    struct {
        Node *case_node;
        long long *jump_addr;
    } case_table[MAX_CASES];
    int num_entries = 0;

    // Check if this is a default-only switch (no case labels)
    if (!node->case_next) {
        // No case labels - just generate default if present
        if (node->default_case) {
            gen_stmt(vm, node->default_case->lhs);
        }
        return;
    }

    // Generate comparison chain
    for (Node *c = node->case_next; c; c = c->case_next) {
        for (long val = c->begin; val <= c->end; val++) {
            // Evaluate switch condition
            gen_expr(vm, node->cond);

            // Compare with case value
            emit(vm, PUSH);
            emit_with_arg(vm, IMM, val);
            emit(vm, EQ);

            // Jump if equal - reserve space for jump address
            emit(vm, JNZ);
            if (num_entries >= MAX_CASES) {
                error_tok(vm, node->tok, "too many case labels");
            }
            case_table[num_entries].case_node = c;
            case_table[num_entries].jump_addr = ++vm->text_ptr;  // Point to jump address slot
            num_entries++;
        }
    }

    // No match - jump to default or end
    emit(vm, JMP);
    long long *no_match_addr = ++vm->text_ptr;

    // Set up sparse switch tracking
    // Record case positions as we generate the body
    long long *old_sparse_case_table = vm->compiler.current_sparse_case_table;
    int old_sparse_num = vm->compiler.current_sparse_num;
    vm->compiler.current_sparse_case_table = case_table[0].jump_addr;
    vm->compiler.current_sparse_num = num_entries;

    // Store mapping in temporary storage for ND_CASE to use
    for (int i = 0; i < num_entries; i++) {
        vm->compiler.sparse_case_nodes[i] = case_table[i].case_node;
        vm->compiler.sparse_jump_addrs[i] = case_table[i].jump_addr;
    }

    // Generate the entire switch body
    // Case labels will patch jump addresses as they're encountered
    gen_stmt(vm, node->then);

    // Restore sparse switch context
    vm->compiler.current_sparse_case_table = old_sparse_case_table;
    vm->compiler.current_sparse_num = old_sparse_num;

    // Default case or end
    if (node->default_case) {
        *no_match_addr = (long long)(vm->text_ptr + 1);
        // Default case is part of node->then, don't generate separately
    } else {
        *no_match_addr = (long long)(vm->text_ptr + 1);
    }
}

// Generate code for a statement
static void gen_stmt(JCC *vm, Node *node) {
    if (!node) {
        return;
    }

    // Emit debug info for source mapping (if debugger enabled)
    emit_debug_info(vm, node->tok);

    switch (node->kind) {
        case ND_EXPR_STMT:
            gen_expr(vm, node->lhs);
            return;

        case ND_RETURN:
            if (node->lhs) {
                // If returning struct/union, copy to return buffer
                if (node->lhs->ty && (node->lhs->ty->kind == TY_STRUCT || node->lhs->ty->kind == TY_UNION)) {
                    // Get next buffer from pool (rotating)
                    char *buffer = vm->compiler.return_buffer_pool[vm->compiler.return_buffer_index];
                    vm->compiler.return_buffer_index = (vm->compiler.return_buffer_index + 1) % RETURN_BUFFER_POOL_SIZE;

                    // MCPY expects stack: sp[0]=size, sp[1]=src, sp[2]=dest
                    // Push order: dest, src, size

                    // Push destination (buffer) FIRST
                    emit_with_arg(vm, IMM, (long long)buffer);
                    emit(vm, PUSH);  // stack = [dest]

                    // Now evaluate source expression (struct to return)
                    gen_expr(vm, node->lhs);  // ax = src address
                    emit(vm, PUSH);  // stack = [dest, src]

                    // Push size
                    emit_with_arg(vm, IMM, node->lhs->ty->size);
                    emit(vm, PUSH);  // stack = [dest, src, size]

                    // Call MCPY (VM memcpy): sp[2]=dest, sp[1]=src, sp[0]=size
                    emit(vm, MCPY);  // Pops size, src, dest; returns dest in ax

                    // Return address of buffer in ax
                    emit_with_arg(vm, IMM, (long long)buffer);
                } else {
                    // For non-struct types, just evaluate and return
                    gen_expr(vm, node->lhs);
                }
            }
            // Clean up VLA allocations before return
            emit_vla_cleanup(vm);
            // Emit SCOPEOUT for function scope before return
            if (vm->flags & JCC_STACK_INSTR) {
                emit_with_arg(vm, SCOPEOUT, vm->current_function_scope_id);
            }
            emit(vm, LEV3);  // Return from function (copies ax to REG_A0)
            return;

        case ND_BLOCK: {
            // Emit SCOPEIN for this block (nested scope)
            int block_scope_id = -1;
            if (vm->flags & JCC_STACK_INSTR) {
                block_scope_id = vm->current_scope_id++;
                emit_with_arg(vm, SCOPEIN, block_scope_id);
            }

            // Generate block body
            for (Node *n = node->body; n; n = n->next) {
                gen_stmt(vm, n);
            }

            // Emit SCOPEOUT for this block
            if (vm->flags & JCC_STACK_INSTR && block_scope_id >= 0) {
                emit_with_arg(vm, SCOPEOUT, block_scope_id);
            }
            return;
        }

        case ND_IF: {
            gen_expr(vm, node->cond);
            
            // JZ (jump if zero) to else or end
            emit(vm, JZ);
            long long *jz_addr = ++vm->text_ptr;
            *jz_addr = 0;  // Write placeholder

            gen_stmt(vm, node->then);

            if (node->els) {
                // Jump over else block
                emit(vm, JMP);
                long long *jmp_addr = ++vm->text_ptr;
                *jmp_addr = 0;  // Write placeholder

                // Patch JZ to jump here
                *jz_addr = (long long)(vm->text_ptr + 1);

                gen_stmt(vm, node->els);

                // Patch JMP to jump here
                *jmp_addr = (long long)(vm->text_ptr + 1);
            } else {
                // Patch JZ to jump here
                *jz_addr = (long long)(vm->text_ptr + 1);
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
            if (node->cond) {
                gen_expr(vm, node->cond);
                emit(vm, JZ);
                long long *jz_addr = ++vm->text_ptr;
                *jz_addr = 0;  // Write placeholder

                // Body
                gen_stmt(vm, node->then);

                // Continue target: jump here to continue loop
                if (node->cont_label) {
                    make_label(vm, node, NULL, node->cont_label);
                }

                // Increment
                if (node->inc) {
                    gen_expr(vm, node->inc);
                }
                
                // Jump back to condition
                emit_with_arg(vm, JMP, (long long)loop_start);

                // Patch JZ to exit loop
                *jz_addr = (long long)(vm->text_ptr + 1);
                
                // Break target: jump here to break out of loop
                if (node->brk_label) {
                    make_label(vm, node, NULL, node->brk_label);
                }
            } else {
                // Infinite loop
                gen_stmt(vm, node->then);
                
                // Continue target
                if (node->cont_label) {
                    make_label(vm, node, NULL, node->cont_label);
                }
                
                if (node->inc) {
                    gen_expr(vm, node->inc);
                }

                emit_with_arg(vm, JMP, (long long)loop_start);
                
                // Break target (unreachable in infinite loop, but needed for break statements)
                if (node->brk_label) {
                    make_label(vm, node, NULL, node->brk_label);
                }
            }
            return;
        }

        case ND_DO: {
            // Do-while loop: execute body first, then check condition
            // Pattern: do { body } while (condition);
            
            // Remember where loop body starts
            long long *loop_start = vm->text_ptr + 1;
            
            // Generate body (always executes at least once)
            gen_stmt(vm, node->then);
            
            // Continue target: jump here for continue statement
            if (node->cont_label) {
                make_label(vm, node, NULL, node->cont_label);
            }
            
            // Evaluate condition
            gen_expr(vm, node->cond);
            
            // Jump back to start if condition is true (non-zero)
            emit_with_arg(vm, JNZ, (long long)loop_start);
            
            // Break target: jump here to break out of loop
            if (node->brk_label) {
                make_label(vm, node, NULL, node->brk_label);
            }
            
            return;
        }

        case ND_SWITCH: {
            // Switch statement: use jump table optimization for dense switches,
            // optimized linear search for sparse switches

            long min_case, max_case;
            int num_cases;

            if (is_dense_switch(node, &min_case, &max_case, &num_cases)) {
                // Dense switch: use jump table for O(1) lookup
                gen_dense_switch(vm, node, min_case, max_case);
            } else {
                // Sparse switch: use optimized linear search (evaluate condition once)
                gen_sparse_switch(vm, node);
            }

            // Break target: jump here to break out of switch
            if (node->brk_label) {
                make_label(vm, node, NULL, node->brk_label);
            }

            return;
        }
        
        case ND_CASE:
            // Case label: record position in jump table for switch dispatch
            // Skip default case - it doesn't fill the jump table
            if (node == vm->compiler.current_switch_default) {
                gen_stmt(vm, node->lhs);
                return;
            }

            if (vm->compiler.current_switch_table) {
                // Dense switch: fill jump table entries for this case's value range
                long long *case_addr = vm->text_ptr + 1;
                for (long val = node->begin; val <= node->end; val++) {
                    long idx = val - vm->compiler.current_switch_min;
                    if (idx >= 0 && idx < vm->compiler.current_switch_size) {
                        vm->compiler.current_switch_table[idx] = (long long)case_addr;
                    }
                }
            } else if (vm->compiler.current_sparse_num > 0) {
                // Sparse switch: patch jump addresses for this case
                long long *case_addr = vm->text_ptr + 1;
                for (int i = 0; i < vm->compiler.current_sparse_num; i++) {
                    if (vm->compiler.sparse_case_nodes[i] == node) {
                        *vm->compiler.sparse_jump_addrs[i] = (long long)case_addr;
                    }
                }
            }
            // Generate the body code (just the first statement after the label)
            // Fallthrough works because subsequent statements are naturally generated
            gen_stmt(vm, node->lhs);
            return;

        case ND_LABEL:
            // Labeled statement: store label address and generate body
            make_label(vm, node, node->label, node->unique_label);
            // Generate the labeled statement
            gen_stmt(vm, node->lhs);
            return;

        case ND_GOTO:
            // Goto statement: emit JMP and record for later patching
            if (vm->compiler.num_goto_patches >= MAX_LABELS) {
                error_tok(vm, node->tok, "too many goto statements");
            }

            // Emit JMP instruction with placeholder address
            emit(vm, JMP);
            long long *jmp_addr = ++vm->text_ptr;

            // Record patch information
            vm->compiler.goto_patches[vm->compiler.num_goto_patches].name = node->label;
            vm->compiler.goto_patches[vm->compiler.num_goto_patches].unique_label = node->unique_label;
            vm->compiler.goto_patches[vm->compiler.num_goto_patches].location = jmp_addr;
            vm->compiler.num_goto_patches++;

            // Placeholder (will be patched after function code generation completes)
            // This is patched in gen_function() after all labels are collected
            *jmp_addr = 0;
            return;

        case ND_GOTO_EXPR:
            // Computed goto: goto *expr
            // Evaluate expression to get target address
            gen_expr(vm, node->lhs);
            // ax now contains the target address
            emit(vm, JMPI);  // Jump indirect to address in ax
            return;

        case ND_ASM:
            // Inline assembly statement
            // Since this is a VM and not native code generation, we provide a callback
            // mechanism for users to handle assembly strings in application-specific ways.
            // If no callback is set, the asm statement is silently ignored (no-op).
            if (vm->compiler.asm_callback) {
                vm->compiler.asm_callback(vm, node->asm_str, vm->compiler.asm_user_data);
            }
            // If no callback: asm statements have no effect in the VM
            return;

        default:
            error_tok(vm, node->tok, "codegen: unsupported statement node kind %d", node->kind);
    }
}

// Add a variable to the debug symbol table
static void add_debug_symbol(JCC *vm, const char *name, long long offset, Type *ty, int is_local) {
    if (!(vm->flags & JCC_ENABLE_DEBUGGER) || vm->dbg.num_debug_symbols >= MAX_DEBUG_SYMBOLS) {
        return;
    }

    vm->dbg.debug_symbols[vm->dbg.num_debug_symbols].name = (char*)name;
    vm->dbg.debug_symbols[vm->dbg.num_debug_symbols].offset = offset;
    vm->dbg.debug_symbols[vm->dbg.num_debug_symbols].ty = ty;
    vm->dbg.debug_symbols[vm->dbg.num_debug_symbols].is_local = is_local;
    vm->dbg.debug_symbols[vm->dbg.num_debug_symbols].scope_depth = vm->current_scope_id;
    vm->dbg.num_debug_symbols++;
}

// Add variable to scope's linked list for efficient iteration
static void add_var_to_scope(JCC *vm, int scope_id, StackVarMeta *meta) {
    // Ensure scope_vars array is large enough
    if (scope_id >= vm->scope_vars_capacity) {
        int new_capacity = scope_id + 16;  // Grow in chunks
        vm->scope_vars = realloc(vm->scope_vars, new_capacity * sizeof(ScopeVarList));
        if (!vm->scope_vars) {
            error("Failed to allocate scope_vars array");
        }
        // Zero-initialize new entries
        for (int i = vm->scope_vars_capacity; i < new_capacity; i++) {
            vm->scope_vars[i].head = NULL;
            vm->scope_vars[i].tail = NULL;
        }
        vm->scope_vars_capacity = new_capacity;
    }

    // Create new node
    ScopeVarNode *node = malloc(sizeof(ScopeVarNode));
    if (!node) {
        error("Failed to allocate ScopeVarNode");
    }
    node->meta = meta;
    node->next = NULL;

    // Append to scope's list (O(1) with tail pointer)
    ScopeVarList *list = &vm->scope_vars[scope_id];
    if (list->tail) {
        list->tail->next = node;
        list->tail = node;
    } else {
        list->head = list->tail = node;
    }
}

// Add stack variable metadata for instrumentation

static void add_stack_var_meta(JCC *vm, const char *name, long long offset, Type *ty, int scope_id) {
    if (!(vm->flags & JCC_STACK_INSTR)) {
        return;
    }

    // Create metadata structure
    StackVarMeta *meta = calloc(1, sizeof(StackVarMeta));
    if (!meta)
        error("out of memory");

    meta->name = (char*)name;
    meta->bp = 0;  // Will be set at runtime
    meta->offset = offset;
    meta->ty = ty;
    meta->scope_id = scope_id;
    meta->is_alive = 0;  // Will be activated by SCOPEIN
    meta->initialized = 0;
    meta->read_count = 0;
    meta->write_count = 0;

    // Store in HashMap keyed by offset (will be bp+offset at runtime)
    // For now, use offset as key during codegen, runtime will use bp+offset
    char key[32];
    snprintf(key, sizeof(key), "%lld", offset);
    // Heap-allocate key since HashMap stores pointer (consistent with other HashMap usage)
    hashmap_put(&vm->stack_var_meta, strdup(key), meta);
    
    // Add to scope's variable list for efficient iteration
    add_var_to_scope(vm, scope_id, meta);
}


// Generate code for a function
// Made non-static for K macro compilation
void gen_function(JCC *vm, Obj *fn) {
    if (!fn->is_function) {
        return;
    }

    // Skip function declarations without body
    if (!fn->body) {
        return;
    }

    // Reset label and goto tables for this function
    vm->compiler.num_labels = 0;
    vm->compiler.num_goto_patches = 0;

    // Set current function for VLA cleanup
    vm->compiler.current_codegen_fn = fn;

    // Save number of global debug symbols (don't clear globals)
    // We only clear locals when entering a new function
    int num_global_symbols = 0;
    if (vm->flags & JCC_ENABLE_DEBUGGER) {
        // Count global symbols (is_local == 0)
        for (int i = 0; i < vm->dbg.num_debug_symbols; i++) {
            if (!vm->dbg.debug_symbols[i].is_local) {
                num_global_symbols++;
            }
        }
        // Reset to globals only (removes previous function's locals)
        vm->dbg.num_debug_symbols = num_global_symbols;
    }

    // Generating function code

    // Register-based calling convention:
    // Arguments are passed in REG_A0-REG_A7 and stored at negative offsets
    // Stack layout after CALL and ENT3:
    // [old_bp]    <- bp[0] or bp (BP points here)
    // [canary]    <- bp[-1] (if canaries enabled)
    // [param1]    <- bp[-1] or bp[-2] with canary (first param from REG_A0)
    // [param2]    <- bp[-2] or bp[-3] with canary (second param from REG_A1)
    // [local1]    <- after params
    // ...
    
    // Function scope (scope_id 0 reserved for function-level)
    int function_scope_id = vm->current_scope_id++;
    vm->current_function_scope_id = function_scope_id;

    // Count parameters first
    int param_count = 0;
    for (Obj *param = fn->params; param; param = param->next) {
        param_count++;
    }
    
    // For variadic functions, we need to copy all 8 potential arg registers
    // because we don't know at compile time how many varargs will be passed.
    // The caller may pass more args than declared fixed params.
    bool is_variadic = fn->ty && fn->ty->is_variadic;
    int reg_param_count = is_variadic ? 8 : param_count;
    
    // Stack size starts with space for parameters (at negative offsets)
    // For variadic functions, reserve space for all 8 potential args
    int stack_size = reg_param_count;
    
    // Assign parameter offsets (negative, after canary slot if enabled)
    // Parameters: bp[-1], bp[-2], ... (or bp[-2], bp[-3] with canary)
    int param_offset = -1;  // First param at bp[-1]
    for (Obj *param = fn->params; param; param = param->next) {
        param->offset = param_offset;
        param->is_local = true;  // Parameters are accessed like locals
        param->is_param = true;  // Mark as parameter for codegen
        // Add to debug symbol table
        add_debug_symbol(vm, param->name, param_offset, param->ty, 1);
        // Add to stack instrumentation metadata
        add_stack_var_meta(vm, param->name, param_offset, param->ty, function_scope_id);
        param_offset--;  // Next param is at lower index
    }
    
    // Count local variables (excluding parameters)
    // For arrays, we need to allocate space for all elements
    for (Obj *var = fn->locals; var; var = var->next) {
        // Check if this is a parameter (params are also in locals list)
        bool is_param = false;
        for (Obj *p = fn->params; p; p = p->next) {
            if (p == var) {
                is_param = true;
                break;
            }
        }
        
        // Skip builtin variables (va_area and alloca_bottom)
        bool is_builtin = (var == fn->va_area) || (var == fn->alloca_bottom);
        
        if (!is_param && !is_builtin) {
            // Calculate how many slots this variable needs
            // VM uses 8-byte slots, so we divide type size by 8
            int var_size = 1;
            if (var->ty->kind == TY_ARRAY) {
                // Arrays: allocate space based on total size in bytes
                // Array size is already computed correctly in ty->size
                var_size = (var->ty->size + 7) / 8;  // Round up to 8-byte slots
            } else if (var->ty->kind == TY_VLA) {
                // VLAs: allocate space for the pointer (1 slot)
                // The actual array memory is allocated by MALC (malloc) at runtime
                var_size = 1;
            } else if (var->ty->kind == TY_STRUCT || var->ty->kind == TY_UNION) {
                // Structs/unions: allocate based on their size
                // Size is in bytes, divide by 8 to get number of 8-byte slots
                var_size = (var->ty->size + 7) / 8;  // Round up
            }
            stack_size += var_size;
            var->offset = -stack_size;  // Locals have negative offsets (below bp and params)
            // Add to debug symbol table
            add_debug_symbol(vm, var->name, var->offset, var->ty, 1);
            // Add to stack instrumentation metadata
            add_stack_var_meta(vm, var->name, var->offset, var->ty, function_scope_id);
        }
    }

    // Store function entry address (right before ENT3)
    // text_ptr is currently pointing to last written instruction
    // Next emit will write at text_ptr+1
    fn->code_addr = (vm->text_ptr + 1 - vm->text_seg);

    // Compute float parameter mask - bit i is set if param i is a float/double
    long long float_param_mask = 0;
    int pindex = 0;
    for (Obj *param = fn->params; param && pindex < 8; param = param->next, pindex++) {
        if (is_flonum(param->ty)) {
            float_param_mask |= (1LL << pindex);
        }
    }

    // Function prologue using ENT3 (register-based calling convention)
    // ENT3 format: [stack_size:32|param_count:32] [float_param_mask]
    // For variadic functions, reg_param_count is 8 to copy all potential args
    long long ent3_operand = ((long long)stack_size) | (((long long)reg_param_count) << 32);
    emit_with_arg(vm, ENT3, ent3_operand);
    *++vm->text_ptr = float_param_mask;  // Second operand: which params are floats

    // Emit SCOPEIN for function-level scope (activates all function variables)
    if (vm->flags & JCC_STACK_INSTR) {
        emit_with_arg(vm, SCOPEIN, function_scope_id);
    }


    // Mark function parameters as initialized (for uninitialized detection)
    if (vm->flags & JCC_UNINIT_DETECTION) {
        for (Obj *param = fn->params; param; param = param->next) {
            // Only mark scalar types (arrays/structs don't need tracking)
            bool is_scalar = (param->ty->kind != TY_ARRAY &&
                              param->ty->kind != TY_STRUCT &&
                              param->ty->kind != TY_UNION);
            if (is_scalar) {
                emit_with_arg(vm, MARKI, param->offset);
            }
        }
    }

    // Generate function body
    gen_stmt(vm, fn->body);

    // Ensure function ends with LEV if no explicit return
    // Note: We don't emit VLA cleanup here because:
    // 1. Functions with explicit returns handle cleanup before each return
    // 2. Functions without explicit returns reaching here is undefined behavior
    //    for non-void functions, so VLA cleanup is not critical
    // Emit SCOPEOUT for function scope before final return
    if (vm->flags & JCC_STACK_INSTR) {
        emit_with_arg(vm, SCOPEOUT, function_scope_id);
    }
    emit(vm, LEV3);  // Implicit return (copies ax to REG_A0)
    
    // Clear current function
    vm->compiler.current_codegen_fn = NULL;

    // Patch all goto statements in this function
    for (int i = 0; i < vm->compiler.num_goto_patches; i++) {
        GotoPatch *patch = &vm->compiler.goto_patches[i];

        // Find the target label
        LabelEntry *target = NULL;
        for (int j = 0; j < vm->compiler.num_labels; j++) {
            // Match either by label name or unique_label (for break/continue)
            if ((patch->name && vm->compiler.label_table[j].name && 
                 strcmp(patch->name, vm->compiler.label_table[j].name) == 0) ||
                (patch->unique_label && vm->compiler.label_table[j].unique_label &&
                 strcmp(patch->unique_label, vm->compiler.label_table[j].unique_label) == 0)) {
                target = &vm->compiler.label_table[j];
                break;
            }
        }
        
        if (!target) {
            error("undefined label: %s", patch->name ? patch->name : patch->unique_label);
        }
        
        // Patch the JMP instruction with the label address
        *patch->location = (long long)target->address;
    }
}

// Main code generation entry point
void codegen(JCC *vm, Obj *prog) {
    if (!vm) {
        error("codegen: VM instance is NULL");
    }
    
    // Initialize text pointer to start of text segment
    // Reserve text_seg[0] for main entry point
    vm->text_ptr = vm->text_seg;  // Start at text_seg[1] (after first emit)
    vm->compiler.num_call_patches = 0;  // Reset patch counter

    // Validate all extern declarations have definitions
    for (Obj *obj = prog; obj; obj = obj->next) {
        if (obj->is_function) {
            // Functions: must have body if not extern
            if (!obj->is_definition && !obj->body) {
                // This is an extern function declaration - that's OK
                // Will be caught later if called but undefined
                continue;
            }
        } else {
            // Variables: check if extern declaration without definition
            // An extern variable should either:
            // 1. Have is_definition = true (it's actually defined here), OR
            // 2. Have init_data (initialized), OR
            // 3. Be tentative (uninitialized non-extern declaration)
            if (!obj->is_definition && !obj->is_tentative && !obj->init_data) {
                // This is an extern variable with no definition
                if (obj->tok) {
                    error_tok(vm, obj->tok, "undefined reference to '%s'", obj->name);
                } else {
                    error("undefined reference to '%s'", obj->name);
                }
            }
        }
    }

    // Initialize all global variables in data segment
    // Process all global variables (both initialized and uninitialized)
    for (Obj *var = prog; var; var = var->next) {
        if (!var->is_function) {
            // Align data pointer (optional but good practice)
            // Round up to 8-byte boundary for efficiency
            long long offset = vm->data_ptr - vm->data_seg;
            offset = (offset + 7) & ~7;
            vm->data_ptr = vm->data_seg + offset;
            
            // Store the offset in the variable
            var->offset = vm->data_ptr - vm->data_seg;

            // Add to debug symbol table (globals are function-independent, so add before functions)
            add_debug_symbol(vm, var->name, var->offset, var->ty, 0);

            // Initialize the data
            // NOTE: Type sizes are already adjusted in new_gvar() for VM compatibility
            // (integers are 8 bytes in VM), so init_data is already in the correct format
            if (var->init_data) {
                // Copy initialized data (init_data is already in VM format with correct sizes)
                memcpy(vm->data_ptr, var->init_data, var->ty->size);
                vm->data_ptr += var->ty->size;
            } else {
                // Zero-initialize uninitialized globals (C standard requires this)
                memset(vm->data_ptr, 0, var->ty->size);
                vm->data_ptr += var->ty->size;
            }
        }
    }
    

    
    // Allocate return buffer pool for struct/union returns at end of data segment
    for (int i = 0; i < RETURN_BUFFER_POOL_SIZE; i++) {
        // Align to 8-byte boundary
        long long offset = vm->data_ptr - vm->data_seg;
        offset = (offset + 7) & ~7;
        vm->data_ptr = vm->data_seg + offset;
        vm->compiler.return_buffer_pool[i] = vm->data_ptr;  // Store pointer for codegen
        memset(vm->compiler.return_buffer_pool[i], 0, vm->compiler.return_buffer_size);
        vm->data_ptr += vm->compiler.return_buffer_size;
    }

    // First pass: generate code for all functions
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (fn->is_function && fn->body) {
            gen_function(vm, fn);
        }
    }

    // Second pass: patch function call addresses
    // Note: We must look up functions by name in the merged program list,
    // because the stored Obj* might point to a forward declaration, not the definition
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
            error("undefined function: %s", fn_name);
        }
        
        long long addr = (long long)(vm->text_seg + fn_def->code_addr);
        *loc = addr;
    }

    // Patch function addresses (for function pointers)
    // Same as call patches - must look up by name to handle forward declarations
    for (int i = 0; i < vm->compiler.num_func_addr_patches; i++) {
        char *fn_name = vm->compiler.func_addr_patches[i].function->name;
        long long *loc = vm->compiler.func_addr_patches[i].location;

        // Find the function definition in the program list
        Obj *fn_def = NULL;
        for (Obj *fn = prog; fn; fn = fn->next) {
            if (fn->is_function && fn->body && strcmp(fn->name, fn_name) == 0) {
                fn_def = fn;
                break;
            }
        }

        if (!fn_def) {
            error("undefined function: %s", fn_name);
        }

        long long addr = (long long)(vm->text_seg + fn_def->code_addr);
        *loc = addr;
    }

    // Find main function and store its address
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (fn->is_function && strcmp(fn->name, "main") == 0) {
            // Store main's address at the start of text segment
            vm->text_seg[0] = fn->code_addr;
            return;
        }
    }

    error("main() function not found");
}
