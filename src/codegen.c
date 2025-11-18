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

// Emit appropriate load instruction based on type
// If is_deref is true, this is a pointer dereference and should be checked
static void emit_load(JCC *vm, Type *ty, int is_deref) {
    // If UAF detection, bounds checking, dangling detection, or memory tagging enabled, check pointer before dereferencing
    // Only check on actual dereferences, not when loading pointer values
    if (is_deref && (vm->flags & JCC_POINTER_CHECKS))
        emit(vm, CHKP);  // Check that pointer in ax is valid

    // If alignment checking enabled, check pointer alignment before dereferencing
    if (is_deref && vm->flags & JCC_ALIGNMENT_CHECKS) {
        // Emit alignment check with type size
        size_t type_size = ty->size;
        if (type_size > 1) {  // Only check for types larger than 1 byte
            emit_with_arg(vm, CHKA, type_size);
        }
    }

    // If type checking enabled, check the type on dereference
    if (is_deref && vm->flags & JCC_TYPE_CHECKS) {
        // Emit type check with expected type
        emit_with_arg(vm, CHKT, ty->kind);
    }

    if (ty->kind == TY_CHAR) {
        emit(vm, LC);  // 1 byte - loads and sign extends via C semantics
    } else if (ty->kind == TY_SHORT) {
        emit(vm, LS);  // 2 bytes - loads and sign extends via C semantics
    } else if (ty->kind == TY_INT || ty->kind == TY_ENUM) {
        emit(vm, LW);  // 4 bytes - loads and sign extends via C semantics
    } else if (is_flonum(ty)) {
        emit(vm, FLD); // floating-point
    } else {
        emit(vm, LI);  // 8 bytes (long, pointers)
    }
}

// Emit appropriate store instruction based on type
static void emit_store(JCC *vm, Type *ty) {
    // Note: UAF checks for stores are done inside the store opcodes themselves
    // because the address is on the stack, not in ax
    if (ty->kind == TY_CHAR) {
        emit(vm, SC);  // 1 byte
    } else if (ty->kind == TY_SHORT) {
        emit(vm, SS);  // 2 bytes
    } else if (ty->kind == TY_INT || ty->kind == TY_ENUM) {
        emit(vm, SW);  // 4 bytes
    } else if (is_flonum(ty)) {
        emit(vm, FST); // floating-point
    } else {
        emit(vm, SI);  // 8 bytes (long, pointers)
    }
}

// Emit debug info (source location mapping) for debugger
static void emit_debug_info(JCC *vm, Token *tok) {
    if (!(vm->flags & JCC_ENABLE_DEBUGGER) || !tok || !tok->file) {
        return;
    }

    // Only emit if line changed
    if (tok->file == vm->last_debug_file && tok->line_no == vm->last_debug_line) {
        return;
    }

    // Grow source map if needed
    if (vm->source_map_count >= vm->source_map_capacity) {
        vm->source_map_capacity *= 2;
        SourceMap *new_map = realloc(vm->source_map, vm->source_map_capacity * sizeof(SourceMap));
        if (!new_map) {
            error("could not realloc source map");
        }
        vm->source_map = new_map;
    }

    // Add mapping entry
    vm->source_map[vm->source_map_count].pc_offset = vm->text_ptr - vm->text_seg;
    vm->source_map[vm->source_map_count].file = tok->file;
    vm->source_map[vm->source_map_count].line_no = tok->line_no;
    vm->source_map_count++;

    vm->last_debug_file = tok->file;
    vm->last_debug_line = tok->line_no;
}

static void gen_stmt(JCC *vm, Node *node);

// Helper to resolve global variable to the canonical version in the merged program
// This is needed because AST nodes may reference Objs from original programs,
// but only the merged program's Objs have the correct offsets set by codegen
static Obj *resolve_global_var(JCC *vm, Obj *var) {
    if (!var || var->is_local) {
        return var;
    }
    
    // Look up in the merged program (vm->globals) by name
    for (Obj *g = vm->globals; g; g = g->next) {
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
    
    for (int i = 0; i < vm->ffi_count; i++) {
        if (strcmp(vm->ffi_table[i].name, name) == 0) {
            return i;
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
                if (vm->num_func_addr_patches >= MAX_CALLS) {
                    error("too many function address references");
                }
                vm->func_addr_patches[vm->num_func_addr_patches].location = addr_loc;
                vm->func_addr_patches[vm->num_func_addr_patches].function = node->var;
                vm->num_func_addr_patches++;
            } else if (node->var->is_local) {
                // Local variable or parameter - load address relative to bp
                // Parameters have positive offsets, locals have negative offsets

                // Check if variable is initialized (for uninitialized detection)
                // Only check scalar types (not arrays/structs) that will actually be loaded
                bool is_param = node->var->offset > 0;
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

                emit_with_arg(vm, LEA, node->var->offset);

                // For struct/union parameters (positive offset), we pass by pointer
                // So we need to load the pointer value first
                if (is_param && (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION)) {
                    emit(vm, LI);  // Load the pointer to struct
                    // Now ax contains the struct address
                } else if (is_scalar) {
                    // For arrays/structs/unions (locals), LEA gives us the address
                    // For scalar types, we need to load the value
                    emit_load(vm, node->ty, 0);  // Not a dereference, just loading variable value
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
                // For scalar types, we need to load the value
                if (node->ty->kind != TY_ARRAY &&
                    node->ty->kind != TY_STRUCT &&
                    node->ty->kind != TY_UNION) {
                    emit_load(vm, node->ty, 0);  // Not a dereference, just loading variable value
                }
            }
            return;

        case ND_ASSIGN:
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
                
                // Add member offset
                if (node->lhs->member->offset != 0) {
                    emit(vm, PUSH);  // Save struct address
                    emit_with_arg(vm, IMM, node->lhs->member->offset);
                    emit(vm, ADD);  // ax = struct_addr + offset
                }
                
                emit(vm, PUSH);  // Push member address onto stack
            } else {
                error_tok(vm, node->tok, "invalid lvalue in assignment");
            }
            
            // Evaluate right side - result goes in ax
            gen_expr(vm, node->rhs);
            
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
            } else {
                // For scalar types, emit appropriate store instruction
                // Mark write access for stack instrumentation (before store)
                if (vm->flags & JCC_STACK_INSTR && node->lhs->kind == ND_VAR && node->lhs->var->is_local) {
                    emit_with_arg(vm, MARKW, node->lhs->var->offset);
                }
                emit_store(vm, node->ty);
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
                emit(vm, FPUSH);
                gen_expr(vm, node->rhs);
                emit(vm, FADD);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit(vm, vm->flags & JCC_OVERFLOW_CHECKS ? ADDC : ADD);
            }

            // Check pointer arithmetic if result is a pointer type
            if (vm->flags & JCC_INVALID_ARITH && node->ty->kind == TY_PTR) {
                emit(vm, CHKPA);
            }
            return;

        case ND_SUB:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                emit(vm, FPUSH);
                gen_expr(vm, node->rhs);
                emit(vm, FSUB);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit(vm, vm->flags & JCC_OVERFLOW_CHECKS ? SUBC : SUB);
            }

            // Check pointer arithmetic if result is a pointer type
            if (vm->flags & JCC_INVALID_ARITH && node->ty->kind == TY_PTR) {
                emit(vm, CHKPA);
            }
            return;

        case ND_MUL:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                emit(vm, FPUSH);
                gen_expr(vm, node->rhs);
                emit(vm, FMUL);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit(vm, vm->flags & JCC_OVERFLOW_CHECKS ? MULC : MUL);
            }
            return;

        case ND_DIV:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                emit(vm, FPUSH);
                gen_expr(vm, node->rhs);
                emit(vm, FDIV);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit(vm, vm->flags & JCC_OVERFLOW_CHECKS ? DIVC : DIV);
            }
            return;

        case ND_MOD:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit(vm, MOD);
            return;

        case ND_EQ:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                emit(vm, FPUSH);
                gen_expr(vm, node->rhs);
                emit(vm, FEQ);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit(vm, EQ);
            }
            return;

        case ND_NE:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                emit(vm, FPUSH);
                gen_expr(vm, node->rhs);
                emit(vm, FNE);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit(vm, NE);
            }
            return;

        case ND_LT:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                emit(vm, FPUSH);
                gen_expr(vm, node->rhs);
                emit(vm, FLT);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit(vm, LT);
            }
            return;

        case ND_LE:
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                emit(vm, FPUSH);
                gen_expr(vm, node->rhs);
                emit(vm, FLE);
            } else {
                emit(vm, PUSH);
                gen_expr(vm, node->rhs);
                emit(vm, LE);
            }
            return;

        case ND_BITOR:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit(vm, OR);
            return;

        case ND_BITXOR:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit(vm, XOR);
            return;

        case ND_BITAND:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit(vm, AND);
            return;

        case ND_SHL:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit(vm, SHL);
            return;

        case ND_SHR:
            gen_expr(vm, node->lhs);
            emit(vm, PUSH);
            gen_expr(vm, node->rhs);
            emit(vm, SHR);
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
                    if (vm->num_func_addr_patches >= MAX_CALLS) {
                        error("too many function address references");
                    }
                    vm->func_addr_patches[vm->num_func_addr_patches].location = addr_loc;
                    vm->func_addr_patches[vm->num_func_addr_patches].function = node->lhs->var;
                    vm->num_func_addr_patches++;
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
                
                // Add member offset
                if (node->lhs->member->offset != 0) {
                    emit(vm, PUSH);  // Save struct address
                    emit_with_arg(vm, IMM, node->lhs->member->offset);
                    emit(vm, ADD);  // ax = struct_addr + offset
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
            // For scalar types, load the value from the address
            emit_load(vm, node->ty, 1);  // This IS a dereference - check it!
            return;

        case ND_NEG:
            // Unary minus
            gen_expr(vm, node->lhs);
            if (is_flonum(node->lhs->ty)) {
                emit(vm, FNEG);
            } else {
                // Push operand first, then multiply by -1
                emit(vm, PUSH);              // Push operand onto stack
                emit_with_arg(vm, IMM, -1);  // Load -1 into ax
                emit(vm, MUL);               // Pop operand, compute operand * -1
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
            if (node->lhs->kind == ND_VAR && node->lhs->var == vm->builtin_alloca) {
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
            if (node->lhs->kind == ND_VAR && node->lhs->var == vm->builtin_setjmp) {
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

            if (node->lhs->kind == ND_VAR && node->lhs->var == vm->builtin_longjmp) {
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

            // Function call: push arguments right-to-left, then CALL
            // Struct/union returns handled by copy-before-return in callee
            
            // Count arguments
            int nargs = 0;
            for (Node *arg = node->args; arg; arg = arg->next) {
                nargs++;
            }
            
            // Collect arguments in array for proper left-to-right evaluation
            Node **arg_array = calloc(nargs, sizeof(Node*));
            int i = 0;
            for (Node *arg = node->args; arg; arg = arg->next) {
                arg_array[i++] = arg;
            }
            
            // Push arguments in reverse order (right-to-left)
            for (int j = nargs - 1; j >= 0; j--) {
                Node *arg = arg_array[j];
                
                // For structs/unions, gen_expr already returns the address
                // So we just push the address (pass by implicit pointer)
                gen_expr(vm, arg);
                if (is_flonum(arg->ty)) {
                    emit(vm, FPUSH);
                } else {
                    emit(vm, PUSH);
                }
            }
            
            free(arg_array);
            
            // Determine if this is a direct or indirect function call
            // Direct call: function name is a ND_VAR with is_function=true
            // Indirect call: anything else (function pointer variable, dereferenced pointer, etc.)
            
            // Track if this is an FFI call (CALLF pops args itself)
            bool is_ffi_call = false;
            
            if (node->lhs->kind == ND_VAR && node->lhs->var->is_function) {
                // Direct function call - check if it's an FFI function first
                Obj *fn = node->lhs->var;
                int ffi_idx = find_ffi_function(vm, fn->name);
                
                if (ffi_idx >= 0) {
                    // It's a registered FFI function - use CALLF
                    // For variadic functions, we need to pass the actual argument count
                    // We'll push the arg count before the function index
                    emit_with_arg(vm, IMM, nargs);  // Actual argument count
                    emit(vm, PUSH);                  // Save on stack
                    emit_with_arg(vm, IMM, ffi_idx); // Function index in ax
                    emit(vm, CALLF);
                    is_ffi_call = true;  // CALLF pops its own arguments
                } else {
                    // Regular VM function - use CALL with static address
                    // Emit CALL instruction with function address
                    // The address is stored as offset in text segment
                    emit(vm, CALL);
                    long long *call_addr = ++vm->text_ptr;
                    
                    // Store patch information
                    if (vm->num_call_patches >= MAX_CALLS) {
                        error("too many function calls");
                    }
                    vm->call_patches[vm->num_call_patches].location = call_addr;
                    vm->call_patches[vm->num_call_patches].function = fn;
                    vm->num_call_patches++;
                    
                    // Emit placeholder (will be patched)
                    *call_addr = 0;
                }
            } else {
                // Indirect function call through pointer - use CALLI
                // Evaluate the function expression to get the address
                gen_expr(vm, node->lhs);
                // ax now contains the function address
                emit(vm, CALLI);
            }
            
            // Clean up arguments from stack (but not for CALLF - it does it itself)
            if (nargs > 0 && !is_ffi_call) {
                emit_with_arg(vm, ADJ, nargs);
            }
            
            // Result is in ax
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
            // Add member offset to get member address
            if (node->member->offset != 0) {
                emit(vm, PUSH);  // Save struct address
                emit_with_arg(vm, IMM, node->member->offset);
                emit(vm, ADD);  // ax = struct_addr + offset
            }
            
            // Now ax contains the address of the member
            // Load the value unless it's an array or nested struct
            if (node->ty->kind != TY_ARRAY && node->ty->kind != TY_STRUCT && node->ty->kind != TY_UNION) {
                // Member access involves dereferencing the struct pointer
                emit_load(vm, node->ty, 1);  // This is a dereference
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

        default:
            error_tok(vm, node->tok, "codegen: unsupported expression node kind %d", node->kind);
    }
}

static void make_label(JCC *vm, Node *node, char *label_name, char *unique_label) {
    if (vm->num_labels >= MAX_LABELS)
        error_tok(vm, node->tok, "too many labels");
    vm->label_table[vm->num_labels].name = label_name;
    vm->label_table[vm->num_labels].unique_label = unique_label;
    vm->label_table[vm->num_labels].address = vm->text_ptr + 1;
    vm->num_labels++;
}

// Helper to emit VLA cleanup code  
// Must preserve ax (return value) during cleanup
static void emit_vla_cleanup(JCC *vm) {
    Obj *fn = vm->current_codegen_fn;
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
                    // MCPY expects stack: sp[0]=size, sp[1]=src, sp[2]=dest
                    // Push order: dest, src, size
                    
                    // Push destination (return_buffer) FIRST
                    emit_with_arg(vm, IMM, (long long)vm->return_buffer);
                    emit(vm, PUSH);  // stack = [dest]
                    
                    // Now evaluate source expression (struct to return)
                    gen_expr(vm, node->lhs);  // ax = src address
                    emit(vm, PUSH);  // stack = [dest, src]
                    
                    // Push size
                    emit_with_arg(vm, IMM, node->lhs->ty->size);
                    emit(vm, PUSH);  // stack = [dest, src, size]
                    
                    // Call MCPY (VM memcpy): sp[2]=dest, sp[1]=src, sp[0]=size
                    emit(vm, MCPY);  // Pops size, src, dest; returns dest in ax
                    
                    // Return address of return_buffer in ax
                    emit_with_arg(vm, IMM, (long long)vm->return_buffer);
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
            emit(vm, LEV);  // Return from function
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
            // Switch statement: compare condition value against each case
            // Simple strategy: re-evaluate condition for each case (inefficient but correct)
            
            #define MAX_CASES 256
            struct {
                Node *case_node;
                long long *jump_addr;
            } case_table[MAX_CASES];
            int num_entries = 0;
            
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
            
            // Generate all case bodies and record their start addresses
            // Store actual addresses, not pointers to addresses
            long long case_body_starts[MAX_CASES];
            int case_idx = 0;
            for (Node *c = node->case_next; c; c = c->case_next) {
                // Record where this case body starts (as actual address)
                case_body_starts[case_idx] = (long long)(vm->text_ptr + 1);
                case_idx++;
                gen_stmt(vm, c->lhs);
            }
            
            // Patch all case jumps
            for (int i = 0; i < num_entries; i++) {
                // Find which case this entry belongs to
                int idx = 0;
                for (Node *c = node->case_next; c; c = c->case_next) {
                    if (c == case_table[i].case_node) {
                        *case_table[i].jump_addr = case_body_starts[idx];
                        break;
                    }
                    idx++;
                }
            }
            
            // Generate default case or just end
            if (node->default_case) {
                *no_match_addr = (long long)(vm->text_ptr + 1);
                gen_stmt(vm, node->default_case->lhs);
            } else {
                *no_match_addr = (long long)(vm->text_ptr + 1);
            }
            
            // Break target: jump here to break out of switch
            if (node->brk_label) {
                make_label(vm, node, NULL, node->brk_label);
            }
            
            return;
        }
        
        case ND_CASE:
            // Case labels are handled by the switch statement
            // Just generate the body code
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
            if (vm->num_goto_patches >= MAX_LABELS) {
                error_tok(vm, node->tok, "too many goto statements");
            }
            
            // Emit JMP instruction with placeholder address
            emit(vm, JMP);
            long long *jmp_addr = ++vm->text_ptr;
            
            // Record patch information
            vm->goto_patches[vm->num_goto_patches].name = node->label;
            vm->goto_patches[vm->num_goto_patches].unique_label = node->unique_label;
            vm->goto_patches[vm->num_goto_patches].location = jmp_addr;
            vm->num_goto_patches++;

            // Placeholder (will be patched after function code generation completes)
            // This is patched in gen_function() after all labels are collected
            *jmp_addr = 0;
            return;

        case ND_ASM:
            // Inline assembly statement
            // Since this is a VM and not native code generation, we provide a callback
            // mechanism for users to handle assembly strings in application-specific ways.
            // If no callback is set, the asm statement is silently ignored (no-op).
            if (vm->asm_callback) {
                vm->asm_callback(vm, node->asm_str, vm->asm_user_data);
            }
            // If no callback: asm statements have no effect in the VM
            return;

        default:
            error_tok(vm, node->tok, "codegen: unsupported statement node kind %d", node->kind);
    }
}

// Add a variable to the debug symbol table
static void add_debug_symbol(JCC *vm, const char *name, long long offset, Type *ty, int is_local) {
    if (!(vm->flags & JCC_ENABLE_DEBUGGER) || vm->num_debug_symbols >= MAX_DEBUG_SYMBOLS) {
        return;
    }

    vm->debug_symbols[vm->num_debug_symbols].name = (char*)name;
    vm->debug_symbols[vm->num_debug_symbols].offset = offset;
    vm->debug_symbols[vm->num_debug_symbols].ty = ty;
    vm->debug_symbols[vm->num_debug_symbols].is_local = is_local;
    vm->debug_symbols[vm->num_debug_symbols].scope_depth = vm->current_scope_id;
    vm->num_debug_symbols++;
}

// Add stack variable metadata for instrumentation
static void add_stack_var_meta(JCC *vm, const char *name, long long offset, Type *ty, int scope_id) {
    if (!(vm->flags & JCC_STACK_INSTR)) {
        return;
    }

    // Create metadata structure
    StackVarMeta *meta = calloc(1, sizeof(StackVarMeta));
    if (!meta) {
        error("failed to allocate stack variable metadata");
    }

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
    vm->num_labels = 0;
    vm->num_goto_patches = 0;

    // Set current function for VLA cleanup
    vm->current_codegen_fn = fn;

    // Save number of global debug symbols (don't clear globals)
    // We only clear locals when entering a new function
    int num_global_symbols = 0;
    if (vm->flags & JCC_ENABLE_DEBUGGER) {
        // Count global symbols (is_local == 0)
        for (int i = 0; i < vm->num_debug_symbols; i++) {
            if (!vm->debug_symbols[i].is_local) {
                num_global_symbols++;
            }
        }
        // Reset to globals only (removes previous function's locals)
        vm->num_debug_symbols = num_global_symbols;
    }

    // Generating function code

    // Set up parameter offsets
    // Stack layout after CALL and ENT:
    // Arguments are pushed right-to-left, so:
    //   push arg2, push arg1, CALL, ENT
    // Memory layout (stack grows downward, but we index upward from BP):
    // [arg2]      <- bp[3] or bp + 3
    // [arg1]      <- bp[2] or bp + 2
    // [ret_addr]  <- bp[1] or bp + 1
    // [old_bp]    <- bp[0] or bp (BP points here)
    // [local1]    <- bp[-1] or below BP (allocated by ENT)
    
    // Function scope (scope_id 0 reserved for function-level)
    int function_scope_id = vm->current_scope_id++;
    vm->current_function_scope_id = function_scope_id;

    int param_offset = 2;  // First param at bp+2 (arg1)
    for (Obj *param = fn->params; param; param = param->next) {
        param->offset = param_offset;
        param->is_local = true;  // Parameters are accessed like locals
        // Add to debug symbol table
        add_debug_symbol(vm, param->name, param_offset, param->ty, 1);
        // Add to stack instrumentation metadata
        add_stack_var_meta(vm, param->name, param_offset, param->ty, function_scope_id);
        param_offset++;  // Next param is at higher index
    }
    
    // Count local variables (excluding parameters)
    // For arrays, we need to allocate space for all elements
    int stack_size = 0;
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
            var->offset = -stack_size;  // Locals have negative offsets (below bp)
            // Add to debug symbol table
            add_debug_symbol(vm, var->name, var->offset, var->ty, 1);
            // Add to stack instrumentation metadata
            add_stack_var_meta(vm, var->name, var->offset, var->ty, function_scope_id);
        }
    }

    // Store function entry address (right before ENT)
    // text_ptr is currently pointing to last written instruction
    // Next emit will write at text_ptr+1
    fn->code_addr = (vm->text_ptr + 1 - vm->text_seg);

    // Function prologue
    emit_with_arg(vm, ENT, stack_size);

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
    emit(vm, LEV);
    
    // Clear current function
    vm->current_codegen_fn = NULL;

    // Patch all goto statements in this function
    for (int i = 0; i < vm->num_goto_patches; i++) {
        GotoPatch *patch = &vm->goto_patches[i];

        // Find the target label
        LabelEntry *target = NULL;
        for (int j = 0; j < vm->num_labels; j++) {
            // Match either by label name or unique_label (for break/continue)
            if ((patch->name && vm->label_table[j].name && 
                 strcmp(patch->name, vm->label_table[j].name) == 0) ||
                (patch->unique_label && vm->label_table[j].unique_label &&
                 strcmp(patch->unique_label, vm->label_table[j].unique_label) == 0)) {
                target = &vm->label_table[j];
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
    vm->num_call_patches = 0;  // Reset patch counter

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
    

    
    // Allocate return buffer for struct/union returns at end of data segment
    // Align to 8-byte boundary
    long long offset = vm->data_ptr - vm->data_seg;
    offset = (offset + 7) & ~7;
    vm->data_ptr = vm->data_seg + offset;
    vm->return_buffer = vm->data_ptr;  // Store pointer for codegen
    memset(vm->return_buffer, 0, vm->return_buffer_size);
    vm->data_ptr += vm->return_buffer_size;

    // First pass: generate code for all functions
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (fn->is_function && fn->body) {
            gen_function(vm, fn);
        }
    }

    // Second pass: patch function call addresses
    // Note: We must look up functions by name in the merged program list,
    // because the stored Obj* might point to a forward declaration, not the definition
    for (int i = 0; i < vm->num_call_patches; i++) {
        char *fn_name = vm->call_patches[i].function->name;
        long long *loc = vm->call_patches[i].location;
        
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
    for (int i = 0; i < vm->num_func_addr_patches; i++) {
        Obj *fn = vm->func_addr_patches[i].function;
        long long *loc = vm->func_addr_patches[i].location;
        *loc = (long long)(vm->text_seg + fn->code_addr);
    }

    // Find main function and store its address
    // (Skip this check when compiling pragma macros)
    if (!vm->compiling_pragma_macro) {
        for (Obj *fn = prog; fn; fn = fn->next) {
            if (fn->is_function && strcmp(fn->name, "main") == 0) {
                // Store main's address at the start of text segment
                vm->text_seg[0] = fn->code_addr;
                return;
            }
        }

        error("main() function not found");
    }
}
