/*
 JCC: JIT C Compiler - LLVM Code Generation

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

#ifdef JCC_HAS_LLVM

#include "./internal.h"
#include "jcc.h"

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>

// =============================================================================
// LLVM Type Mapping
// =============================================================================

// Forward declarations
static LLVMTypeRef llvm_type_for(LLVMCodegen *cg, Type *ty);
static void gen_stmt(JCC *vm, LLVMCodegen *cg, Node *node);
static LLVMValueRef gen_expr(JCC *vm, LLVMCodegen *cg, Node *node);
static LLVMValueRef gen_addr(JCC *vm, LLVMCodegen *cg, Node *node);

/*!
 @function llvm_type_for
 @abstract Convert an Assisi Type to an LLVM type.
 @param cg The LLVMCodegen instance.
 @param ty The Assisi type to convert.
 @return The corresponding LLVM type.
 @discussion Handles all Assisi type kinds including self-referential structs
                         via caching. Named structs are cached to prevent infinite
 recursion.
*/
static LLVMTypeRef llvm_type_for(LLVMCodegen *cg, Type *ty) {
    if (!ty) {
        return LLVMInt64TypeInContext(cg->context);
    }
    
    switch (ty->kind) {
    case TY_VOID:
        return LLVMVoidTypeInContext(cg->context);

    case TY_BOOL:
    case TY_CHAR:
        return LLVMInt8TypeInContext(cg->context);

    case TY_SHORT:
        return LLVMInt16TypeInContext(cg->context);

    case TY_INT:
    case TY_ENUM:
        return LLVMInt32TypeInContext(cg->context);

    case TY_LONG:
        return LLVMInt64TypeInContext(cg->context);

    case TY_FLOAT:
        return LLVMFloatTypeInContext(cg->context);

    case TY_DOUBLE:
    case TY_LDOUBLE:
        return LLVMDoubleTypeInContext(cg->context);

    case TY_PTR:
        // LLVM 15+ uses opaque pointers
        return LLVMPointerTypeInContext(cg->context, 0);

    case TY_ARRAY:
        return LLVMArrayType(llvm_type_for(cg, ty->base), ty->array_len);

    case TY_FUNC: {
        // Build function type
        Type *ret_ty = ty->return_ty;
        bool uses_sret = (ret_ty->kind == TY_STRUCT || ret_ty->kind == TY_UNION);
        
        // For struct/union returns, we use sret convention (void return + hidden pointer param)
        LLVMTypeRef actual_ret_ty = uses_sret 
            ? LLVMVoidTypeInContext(cg->context)
            : llvm_type_for(cg, ret_ty);
        
        // Count parameters: add 1 for sret if needed
        int param_count = 0;
        for (Type *p = ty->params; p; p = p->next)
            param_count++;
        if (uses_sret)
            param_count++;

        LLVMTypeRef *param_types = calloc(param_count, sizeof(LLVMTypeRef));
        int i = 0;
        
        // First parameter is sret pointer for aggregate returns
        if (uses_sret) {
            param_types[i++] = LLVMPointerTypeInContext(cg->context, 0);
        }
        
        // For aggregate parameters, use pointer type (byval convention)
        for (Type *p = ty->params; p; p = p->next) {
            if (p->kind == TY_STRUCT || p->kind == TY_UNION) {
                param_types[i++] = LLVMPointerTypeInContext(cg->context, 0);
            } else {
                param_types[i++] = llvm_type_for(cg, p);
            }
        }

        LLVMTypeRef fn_type =
                LLVMFunctionType(actual_ret_ty, param_types, param_count, ty->is_variadic);

        free(param_types);
        return fn_type;
    }

    case TY_STRUCT: {
        // Check cache first (prevents infinite recursion for self-referential
        // structs)
        LLVMTypeRef cached = hashmap_get_int(&cg->struct_types, (long long)ty);
        if (cached)
            return cached;

        // Create named struct type (with empty body initially)
        // Use address as unique name if no tag name available
        char name_buf[64];
        if (ty->name && ty->name->loc) {
            snprintf(name_buf, sizeof(name_buf), "struct.%.*s", ty->name->len,
                             ty->name->loc);
        } else {
            snprintf(name_buf, sizeof(name_buf), "struct.anon.%llx", (long long)ty);
        }
        LLVMTypeRef struct_type = LLVMStructCreateNamed(cg->context, name_buf);

        // Cache immediately BEFORE filling body (prevents recursion)
        hashmap_put_int(&cg->struct_types, (long long)ty, struct_type);

        // Count members and build member types array
        int member_count = 0;
        for (Member *m = ty->members; m; m = m->next)
            member_count++;

        LLVMTypeRef *member_types = calloc(member_count, sizeof(LLVMTypeRef));
        int idx = 0;
        for (Member *m = ty->members; m; m = m->next)
            member_types[idx++] = llvm_type_for(cg, m->ty);

        // Set struct body with packed flag
        LLVMStructSetBody(struct_type, member_types, member_count, ty->is_packed);

        free(member_types);
        return struct_type;
    }

    case TY_UNION: {
        // Check cache first
        LLVMTypeRef cached = hashmap_get_int(&cg->struct_types, (long long)ty);
        if (cached)
            return cached;

        // Union representation: byte array of size ty->size with proper alignment
        // This matches how the frontend calculates union size (largest member)
        LLVMTypeRef byte_type = LLVMInt8TypeInContext(cg->context);
        LLVMTypeRef union_type = LLVMArrayType(byte_type, ty->size);

        // Cache it
        hashmap_put_int(&cg->struct_types, (long long)ty, union_type);

        return union_type;
    }

    case TY_VLA:
        // VLA becomes a pointer
        return LLVMPointerTypeInContext(cg->context, 0);

    case TY_ERROR:
    default:
        // Fallback to i64
        return LLVMInt64TypeInContext(cg->context);
    }
}

// =============================================================================
// Initialization / Cleanup
// =============================================================================

int codegen_llvm_init(LLVMCodegen *cg) {
    memset(cg, 0, sizeof(LLVMCodegen));

    // Initialize LLVM
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    // Create context and module
    cg->context = LLVMContextCreate();
    if (!cg->context)
        return -1;

    cg->module = LLVMModuleCreateWithNameInContext("acc_module", cg->context);
    if (!cg->module) {
        LLVMContextDispose(cg->context);
        return -1;
    }

    // Create IR builder
    cg->builder = LLVMCreateBuilderInContext(cg->context);
    if (!cg->builder) {
        LLVMDisposeModule(cg->module);
        LLVMContextDispose(cg->context);
        return -1;
    }

    return 0;
}

void codegen_llvm_destroy(LLVMCodegen *cg) {
    if (cg->builder) {
        LLVMDisposeBuilder(cg->builder);
        cg->builder = NULL;
    }
    if (cg->module) {
        LLVMDisposeModule(cg->module);
        cg->module = NULL;
    }
    if (cg->context) {
        LLVMContextDispose(cg->context);
        cg->context = NULL;
    }
}

// =============================================================================
// Code Generation - Address Computation
// =============================================================================

/*!
 @function gen_addr
 @abstract Generate the address (lvalue) of a node.
 @discussion Used for assignment targets, address-of operator, and member
 access.
*/
static LLVMValueRef gen_addr(JCC *vm, LLVMCodegen *cg, Node *node) {
    switch (node->kind) {
    case ND_VAR: {
        Obj *var = node->var;
        if (var->is_local) {
            LLVMValueRef alloca = hashmap_get_int(&cg->obj_to_llvm, (long long)var);
            if (alloca)
                return alloca;
        } else {
            LLVMValueRef global = hashmap_get(&cg->obj_to_llvm, var->name);
            if (!global) {
                global = LLVMGetNamedGlobal(cg->module, var->name);
            }
            if (global)
                return global;
        }
        return LLVMConstNull(LLVMPointerTypeInContext(cg->context, 0));
    }

    case ND_DEREF:
        // Deref's address is simply the pointer being dereferenced
        return gen_expr(vm, cg, node->lhs);

    case ND_MEMBER: {
        // Get address of the struct/union
        LLVMValueRef struct_addr = gen_addr(vm, cg, node->lhs);
        Member *mem = node->member;

        // For unions, we just need to cast the base address
        // For structs, we use byte offset calculation
        LLVMTypeRef i8_ty = LLVMInt8TypeInContext(cg->context);
        LLVMTypeRef i32_ty = LLVMInt32TypeInContext(cg->context);

        // Calculate offset in bytes
        LLVMValueRef offset = LLVMConstInt(i32_ty, mem->offset, 0);

        // Use GEP with i8 to add byte offset
        return LLVMBuildGEP2(cg->builder, i8_ty, struct_addr, &offset, 1, "");
    }

    case ND_COMMA:
        gen_expr(vm, cg, node->lhs);
        return gen_addr(vm, cg, node->rhs);

    default:
        // For other expressions, compute the value and hope it's a pointer
        return gen_expr(vm, cg, node);
    }
}

// =============================================================================
// Code Generation - Expressions
// =============================================================================

static LLVMValueRef gen_expr(JCC *vm, LLVMCodegen *cg, Node *node) {
    if (!node)
        return NULL;

    switch (node->kind) {
    case ND_NUM: {
        LLVMTypeRef ty = llvm_type_for(cg, node->ty);
        if (is_flonum(node->ty)) {
            return LLVMConstReal(ty, node->fval);
        } else {
            return LLVMConstInt(ty, node->val, !node->ty->is_unsigned);
        }
    }

    case ND_VAR: {
        Obj *var = node->var;
        LLVMTypeRef ty = llvm_type_for(cg, var->ty);

        if (var->is_local) {
            // Local variable: look up by Obj* pointer
            LLVMValueRef alloca = hashmap_get_int(&cg->obj_to_llvm, (long long)var);
            if (!alloca) {
                // Fallback: shouldn't happen, but return zero for now
                return LLVMConstNull(ty);
            }
            // Array decay: return pointer directly, don't load
            if (var->ty->kind == TY_ARRAY) {
                return alloca;
            }
            return LLVMBuildLoad2(cg->builder, ty, alloca,
                                                        var->name ? var->name : "");
        } else {
            // Global variable: look up by name
            LLVMValueRef global = hashmap_get(&cg->obj_to_llvm, var->name);
            if (!global) {
                // Try to get from module directly
                global = LLVMGetNamedGlobal(cg->module, var->name);
            }
            if (!global) {
                return LLVMConstNull(ty);
            }
            // For arrays, return the pointer directly (array decay)
            if (var->ty->kind == TY_ARRAY) {
                return global;
            }
            return LLVMBuildLoad2(cg->builder, ty, global, var->name);
        }
    }

    case ND_CAST: {
        // Generate the expression being cast
        LLVMValueRef val = gen_expr(vm, cg, node->lhs);
        LLVMTypeRef from_ty = LLVMTypeOf(val);
        LLVMTypeRef to_ty = llvm_type_for(cg, node->ty);

        // If types are the same, no cast needed
        if (from_ty == to_ty) {
            return val;
        }

        // Get type kinds for decision making
        LLVMTypeKind from_kind = LLVMGetTypeKind(from_ty);
        LLVMTypeKind to_kind = LLVMGetTypeKind(to_ty);

        // Handle different cast scenarios
        if (to_kind == LLVMIntegerTypeKind && from_kind == LLVMIntegerTypeKind) {
            // Integer to integer cast
            unsigned from_bits = LLVMGetIntTypeWidth(from_ty);
            unsigned to_bits = LLVMGetIntTypeWidth(to_ty);
            if (to_bits > from_bits) {
                // Extend
                if (node->lhs->ty && node->lhs->ty->is_unsigned) {
                    return LLVMBuildZExt(cg->builder, val, to_ty, "");
                } else {
                    return LLVMBuildSExt(cg->builder, val, to_ty, "");
                }
            } else if (to_bits < from_bits) {
                // Truncate
                return LLVMBuildTrunc(cg->builder, val, to_ty, "");
            } else {
                // Same size, just bitcast
                return val;
            }
        } else if (to_kind == LLVMFloatTypeKind || to_kind == LLVMDoubleTypeKind) {
            // Casting to float/double
            if (from_kind == LLVMIntegerTypeKind) {
                if (node->lhs->ty && node->lhs->ty->is_unsigned) {
                    return LLVMBuildUIToFP(cg->builder, val, to_ty, "");
                } else {
                    return LLVMBuildSIToFP(cg->builder, val, to_ty, "");
                }
            } else if (from_kind == LLVMFloatTypeKind ||
                                 from_kind == LLVMDoubleTypeKind) {
                return LLVMBuildFPCast(cg->builder, val, to_ty, "");
            }
        } else if (from_kind == LLVMFloatTypeKind ||
                             from_kind == LLVMDoubleTypeKind) {
            // Casting from float/double to int
            if (to_kind == LLVMIntegerTypeKind) {
                if (node->ty && node->ty->is_unsigned) {
                    return LLVMBuildFPToUI(cg->builder, val, to_ty, "");
                } else {
                    return LLVMBuildFPToSI(cg->builder, val, to_ty, "");
                }
            }
        } else if (to_kind == LLVMPointerTypeKind) {
            // Cast to pointer (inttoptr or bitcast)
            if (from_kind == LLVMIntegerTypeKind) {
                return LLVMBuildIntToPtr(cg->builder, val, to_ty, "");
            } else if (from_kind == LLVMPointerTypeKind) {
                return val; // Opaque pointers, no cast needed
            }
        } else if (from_kind == LLVMPointerTypeKind &&
                             to_kind == LLVMIntegerTypeKind) {
            // Pointer to int
            return LLVMBuildPtrToInt(cg->builder, val, to_ty, "");
        }

        // Fallback: return the value as-is
        return val;
    }

        // =========================================================================
        // Binary Arithmetic Operators
        // =========================================================================

    case ND_ADD: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);

        // Check for pointer arithmetic
        // NOTE: The parser already scales indices by element size (for bytecode VM)
        // so we use i8 GEP (byte offsets) to avoid double-scaling.
        // The parser may also cast the index to pointer type, so we need to
        // convert it back to integer for the GEP.
        if (node->lhs->ty->base) {
            // Pointer + integer (already scaled to bytes)
            LLVMTypeRef i8_ty = LLVMInt8TypeInContext(cg->context);
            LLVMTypeRef i64_ty = LLVMInt64TypeInContext(cg->context);
            // Convert rhs to integer if it's a pointer (from cast)
            if (LLVMGetTypeKind(LLVMTypeOf(rhs)) == LLVMPointerTypeKind) {
                rhs = LLVMBuildPtrToInt(cg->builder, rhs, i64_ty, "");
            }
            return LLVMBuildGEP2(cg->builder, i8_ty, lhs, &rhs, 1, "");
        }
        if (node->rhs->ty->base) {
            // Integer + pointer (already scaled to bytes)
            LLVMTypeRef i8_ty = LLVMInt8TypeInContext(cg->context);
            LLVMTypeRef i64_ty = LLVMInt64TypeInContext(cg->context);
            // Convert lhs to integer if it's a pointer (from cast)
            if (LLVMGetTypeKind(LLVMTypeOf(lhs)) == LLVMPointerTypeKind) {
                lhs = LLVMBuildPtrToInt(cg->builder, lhs, i64_ty, "");
            }
            return LLVMBuildGEP2(cg->builder, i8_ty, rhs, &lhs, 1, "");
        }

        if (is_flonum(node->ty)) {
            return LLVMBuildFAdd(cg->builder, lhs, rhs, "");
        }
        return LLVMBuildAdd(cg->builder, lhs, rhs, "");
    }

    case ND_SUB: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);

        // Check for pointer arithmetic
        // NOTE: The parser already scales indices by element size (for bytecode VM)
        if (node->lhs->ty->base && !node->rhs->ty->base) {
            // Pointer - integer (already scaled to bytes): negate and use i8 GEP
            LLVMTypeRef i8_ty = LLVMInt8TypeInContext(cg->context);
            LLVMTypeRef i64_ty = LLVMInt64TypeInContext(cg->context);
            // Convert rhs to integer if it's a pointer (from cast)
            if (LLVMGetTypeKind(LLVMTypeOf(rhs)) == LLVMPointerTypeKind) {
                rhs = LLVMBuildPtrToInt(cg->builder, rhs, i64_ty, "");
            }
            LLVMValueRef neg_rhs = LLVMBuildNeg(cg->builder, rhs, "");
            return LLVMBuildGEP2(cg->builder, i8_ty, lhs, &neg_rhs, 1, "");
        }
        if (node->lhs->ty->base && node->rhs->ty->base) {
            // Pointer - pointer: parser already handles division by element size
            // Just do direct byte subtraction
            LLVMTypeRef i64_ty = LLVMInt64TypeInContext(cg->context);
            LLVMValueRef lhs_int = LLVMBuildPtrToInt(cg->builder, lhs, i64_ty, "");
            LLVMValueRef rhs_int = LLVMBuildPtrToInt(cg->builder, rhs, i64_ty, "");
            return LLVMBuildSub(cg->builder, lhs_int, rhs_int, "");
        }

        if (is_flonum(node->ty)) {
            return LLVMBuildFSub(cg->builder, lhs, rhs, "");
        }
        return LLVMBuildSub(cg->builder, lhs, rhs, "");
    }

    case ND_MUL: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        if (is_flonum(node->ty)) {
            return LLVMBuildFMul(cg->builder, lhs, rhs, "");
        }
        return LLVMBuildMul(cg->builder, lhs, rhs, "");
    }

    case ND_DIV: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        if (is_flonum(node->ty)) {
            return LLVMBuildFDiv(cg->builder, lhs, rhs, "");
        }
        if (node->ty->is_unsigned) {
            return LLVMBuildUDiv(cg->builder, lhs, rhs, "");
        }
        return LLVMBuildSDiv(cg->builder, lhs, rhs, "");
    }

    case ND_MOD: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        if (node->ty->is_unsigned) {
            return LLVMBuildURem(cg->builder, lhs, rhs, "");
        }
        return LLVMBuildSRem(cg->builder, lhs, rhs, "");
    }

        // =========================================================================
        // Comparison Operators
        // =========================================================================

    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        LLVMValueRef cmp;

        if (is_flonum(node->lhs->ty)) {
            LLVMRealPredicate pred;
            switch (node->kind) {
            case ND_EQ:
                pred = LLVMRealOEQ;
                break;
            case ND_NE:
                pred = LLVMRealONE;
                break;
            case ND_LT:
                pred = LLVMRealOLT;
                break;
            case ND_LE:
                pred = LLVMRealOLE;
                break;
            default:
                pred = LLVMRealOEQ;
                break;
            }
            cmp = LLVMBuildFCmp(cg->builder, pred, lhs, rhs, "");
        } else {
            LLVMIntPredicate pred;
            bool is_unsigned = node->lhs->ty->is_unsigned;
            switch (node->kind) {
            case ND_EQ:
                pred = LLVMIntEQ;
                break;
            case ND_NE:
                pred = LLVMIntNE;
                break;
            case ND_LT:
                pred = is_unsigned ? LLVMIntULT : LLVMIntSLT;
                break;
            case ND_LE:
                pred = is_unsigned ? LLVMIntULE : LLVMIntSLE;
                break;
            default:
                pred = LLVMIntEQ;
                break;
            }
            cmp = LLVMBuildICmp(cg->builder, pred, lhs, rhs, "");
        }

        // Result is i1, extend to result type (typically i32)
        LLVMTypeRef result_ty = llvm_type_for(cg, node->ty);
        return LLVMBuildZExt(cg->builder, cmp, result_ty, "");
    }

        // =========================================================================
        // Bitwise Operators
        // =========================================================================

    case ND_BITAND: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        return LLVMBuildAnd(cg->builder, lhs, rhs, "");
    }

    case ND_BITOR: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        return LLVMBuildOr(cg->builder, lhs, rhs, "");
    }

    case ND_BITXOR: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        return LLVMBuildXor(cg->builder, lhs, rhs, "");
    }

    case ND_SHL: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        return LLVMBuildShl(cg->builder, lhs, rhs, "");
    }

    case ND_SHR: {
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        // Arithmetic shift (AShr) for signed, logical shift (LShr) for unsigned
        if (node->lhs->ty->is_unsigned) {
            return LLVMBuildLShr(cg->builder, lhs, rhs, "");
        }
        return LLVMBuildAShr(cg->builder, lhs, rhs, "");
    }

        // =========================================================================
        // Logical Operators (Short-Circuit)
        // =========================================================================

    case ND_LOGAND: {
        // Short-circuit AND: if lhs is false, skip rhs
        LLVMBasicBlockRef rhs_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "and.rhs");
        LLVMBasicBlockRef merge_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "and.merge");

        // Evaluate lhs
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef lhs_bool = LLVMBuildICmp(
                cg->builder, LLVMIntNE, lhs, LLVMConstInt(LLVMTypeOf(lhs), 0, 0), "");
        LLVMBasicBlockRef lhs_bb = LLVMGetInsertBlock(cg->builder);
        LLVMBuildCondBr(cg->builder, lhs_bool, rhs_bb, merge_bb);

        // Evaluate rhs (only if lhs was true)
        LLVMPositionBuilderAtEnd(cg->builder, rhs_bb);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        LLVMValueRef rhs_bool = LLVMBuildICmp(
                cg->builder, LLVMIntNE, rhs, LLVMConstInt(LLVMTypeOf(rhs), 0, 0), "");
        rhs_bb = LLVMGetInsertBlock(cg->builder);
        LLVMBuildBr(cg->builder, merge_bb);

        // Merge with phi
        LLVMPositionBuilderAtEnd(cg->builder, merge_bb);
        LLVMTypeRef i1_ty = LLVMInt1TypeInContext(cg->context);
        LLVMValueRef phi = LLVMBuildPhi(cg->builder, i1_ty, "");
        LLVMValueRef incoming_vals[] = {LLVMConstInt(i1_ty, 0, 0), rhs_bool};
        LLVMBasicBlockRef incoming_bbs[] = {lhs_bb, rhs_bb};
        LLVMAddIncoming(phi, incoming_vals, incoming_bbs, 2);

        // Extend to result type
        LLVMTypeRef result_ty = llvm_type_for(cg, node->ty);
        return LLVMBuildZExt(cg->builder, phi, result_ty, "");
    }

    case ND_LOGOR: {
        // Short-circuit OR: if lhs is true, skip rhs
        LLVMBasicBlockRef rhs_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "or.rhs");
        LLVMBasicBlockRef merge_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "or.merge");

        // Evaluate lhs
        LLVMValueRef lhs = gen_expr(vm, cg, node->lhs);
        LLVMValueRef lhs_bool = LLVMBuildICmp(
                cg->builder, LLVMIntNE, lhs, LLVMConstInt(LLVMTypeOf(lhs), 0, 0), "");
        LLVMBasicBlockRef lhs_bb = LLVMGetInsertBlock(cg->builder);
        LLVMBuildCondBr(cg->builder, lhs_bool, merge_bb, rhs_bb);

        // Evaluate rhs (only if lhs was false)
        LLVMPositionBuilderAtEnd(cg->builder, rhs_bb);
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        LLVMValueRef rhs_bool = LLVMBuildICmp(
                cg->builder, LLVMIntNE, rhs, LLVMConstInt(LLVMTypeOf(rhs), 0, 0), "");
        rhs_bb = LLVMGetInsertBlock(cg->builder);
        LLVMBuildBr(cg->builder, merge_bb);

        // Merge with phi
        LLVMPositionBuilderAtEnd(cg->builder, merge_bb);
        LLVMTypeRef i1_ty = LLVMInt1TypeInContext(cg->context);
        LLVMValueRef phi = LLVMBuildPhi(cg->builder, i1_ty, "");
        LLVMValueRef incoming_vals[] = {LLVMConstInt(i1_ty, 1, 0), rhs_bool};
        LLVMBasicBlockRef incoming_bbs[] = {lhs_bb, rhs_bb};
        LLVMAddIncoming(phi, incoming_vals, incoming_bbs, 2);

        // Extend to result type
        LLVMTypeRef result_ty = llvm_type_for(cg, node->ty);
        return LLVMBuildZExt(cg->builder, phi, result_ty, "");
    }

        // =========================================================================
        // Unary Operators
        // =========================================================================

    case ND_NEG: {
        LLVMValueRef val = gen_expr(vm, cg, node->lhs);
        if (is_flonum(node->ty)) {
            return LLVMBuildFNeg(cg->builder, val, "");
        }
        return LLVMBuildNeg(cg->builder, val, "");
    }

    case ND_NOT: {
        // Logical NOT: !x is equivalent to x == 0
        LLVMValueRef val = gen_expr(vm, cg, node->lhs);
        LLVMValueRef zero = LLVMConstInt(LLVMTypeOf(val), 0, 0);
        LLVMValueRef cmp = LLVMBuildICmp(cg->builder, LLVMIntEQ, val, zero, "");
        LLVMTypeRef result_ty = llvm_type_for(cg, node->ty);
        return LLVMBuildZExt(cg->builder, cmp, result_ty, "");
    }

    case ND_BITNOT: {
        LLVMValueRef val = gen_expr(vm, cg, node->lhs);
        return LLVMBuildNot(cg->builder, val, "");
    }

    case ND_ADDR: {
        // Address-of: return the address without loading
        return gen_addr(vm, cg, node->lhs);
    }

    case ND_DEREF: {
        // Dereference: load from pointer
        LLVMValueRef ptr = gen_expr(vm, cg, node->lhs);
        LLVMTypeRef ty = llvm_type_for(cg, node->ty);
        // Don't load if dereferencing to array (array decay)
        if (node->ty->kind == TY_ARRAY || node->ty->kind == TY_STRUCT ||
                node->ty->kind == TY_UNION) {
            return ptr;
        }
        return LLVMBuildLoad2(cg->builder, ty, ptr, "");
    }

        // =========================================================================
        // Assignment
        // =========================================================================

    case ND_ASSIGN: {
        LLVMValueRef addr = gen_addr(vm, cg, node->lhs);
        Type *ty = node->lhs->ty;
        
        if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
            // Aggregate assignment requires memcpy
            LLVMValueRef src = gen_expr(vm, cg, node->rhs);  // Returns pointer for aggregates
            LLVMTypeRef i64_ty = LLVMInt64TypeInContext(cg->context);
            LLVMValueRef size = LLVMConstInt(i64_ty, ty->size, 0);
            unsigned align = ty->align > 0 ? ty->align : 1;
            LLVMBuildMemCpy(cg->builder, addr, align, src, align, size);
            return addr;
        }
        
        LLVMValueRef rhs = gen_expr(vm, cg, node->rhs);
        LLVMBuildStore(cg->builder, rhs, addr);
        return rhs;
    }

        // =========================================================================
        // Ternary / Conditional
        // =========================================================================

    case ND_COND: {
        LLVMValueRef cond = gen_expr(vm, cg, node->cond);
        LLVMValueRef cond_bool = LLVMBuildICmp(
                cg->builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, 0), "");

        LLVMBasicBlockRef then_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "cond.then");
        LLVMBasicBlockRef else_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "cond.else");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
                cg->context, cg->current_fn, "cond.merge");

        LLVMBuildCondBr(cg->builder, cond_bool, then_bb, else_bb);

        // Generate then expression
        LLVMPositionBuilderAtEnd(cg->builder, then_bb);
        LLVMValueRef then_val = gen_expr(vm, cg, node->then);
        then_bb = LLVMGetInsertBlock(cg->builder);
        LLVMBuildBr(cg->builder, merge_bb);

        // Generate else expression
        LLVMPositionBuilderAtEnd(cg->builder, else_bb);
        LLVMValueRef else_val = gen_expr(vm, cg, node->els);
        else_bb = LLVMGetInsertBlock(cg->builder);
        LLVMBuildBr(cg->builder, merge_bb);

        // Merge with phi
        LLVMPositionBuilderAtEnd(cg->builder, merge_bb);
        
        // Use node->ty if available, otherwise infer from then_val
        LLVMTypeRef result_ty;
        if (node->ty && node->ty->size > 0) {
            result_ty = llvm_type_for(cg, node->ty);
        } else {
            result_ty = LLVMTypeOf(then_val);
        }
        
        LLVMValueRef phi = LLVMBuildPhi(cg->builder, result_ty, "");
        LLVMValueRef incoming_vals[] = {then_val, else_val};
        LLVMBasicBlockRef incoming_bbs[] = {then_bb, else_bb};
        LLVMAddIncoming(phi, incoming_vals, incoming_bbs, 2);

        return phi;
    }

        // =========================================================================
        // Function Calls
        // =========================================================================

    case ND_FUNCALL: {
        // Get function from expression (could be direct call or function pointer)
        LLVMValueRef fn = NULL;
        LLVMTypeRef fn_type = NULL;
        Type *callee_ty = NULL;  // The function type itself

        if (node->lhs->kind == ND_VAR && node->lhs->var->is_function) {
            // Direct function call
            fn = LLVMGetNamedFunction(cg->module, node->lhs->var->name);
            if (!fn) {
                fn = hashmap_get(&cg->obj_to_llvm, node->lhs->var->name);
            }
            callee_ty = node->lhs->var->ty;
            fn_type = llvm_type_for(cg, callee_ty);
        } else {
            // Indirect call through function pointer
            fn = gen_expr(vm, cg, node->lhs);
            callee_ty = node->func_ty;
            fn_type = llvm_type_for(cg, callee_ty);
        }

        if (!fn) {
            return LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, 0);
        }

        // Check if function returns aggregate (uses sret)
        Type *ret_ty = node->ty;
        bool uses_sret = (ret_ty->kind == TY_STRUCT || ret_ty->kind == TY_UNION);
        
        // Allocate temp for sret return value if needed
        LLVMValueRef sret_alloca = NULL;
        if (uses_sret) {
            LLVMTypeRef sret_ty = llvm_type_for(cg, ret_ty);
            sret_alloca = LLVMBuildAlloca(cg->builder, sret_ty, "sret.tmp");
            if (ret_ty->align > 0) {
                LLVMSetAlignment(sret_alloca, ret_ty->align);
            }
        }

        // Count arguments: add 1 for sret if needed
        int arg_count = 0;
        for (Node *arg = node->args; arg; arg = arg->next)
            arg_count++;
        if (uses_sret)
            arg_count++;

        LLVMValueRef *args = calloc(arg_count, sizeof(LLVMValueRef));
        int i = 0;
        
        // First argument is sret pointer for aggregate returns
        if (uses_sret) {
            args[i++] = sret_alloca;
        }
        
        // Generate remaining arguments, handling byval for aggregates
        for (Node *arg = node->args; arg; arg = arg->next) {
            Type *arg_ty = arg->ty;
            if (arg_ty->kind == TY_STRUCT || arg_ty->kind == TY_UNION) {
                // Pass aggregate by value: copy to temp alloca, pass pointer
                LLVMValueRef src = gen_addr(vm, cg, arg);
                LLVMTypeRef ty = llvm_type_for(cg, arg_ty);
                LLVMValueRef tmp = LLVMBuildAlloca(cg->builder, ty, "byval.tmp");
                unsigned align = arg_ty->align > 0 ? arg_ty->align : 1;
                LLVMSetAlignment(tmp, align);
                LLVMTypeRef i64_ty = LLVMInt64TypeInContext(cg->context);
                LLVMValueRef size = LLVMConstInt(i64_ty, arg_ty->size, 0);
                LLVMBuildMemCpy(cg->builder, tmp, align, src, align, size);
                args[i++] = tmp;
            } else {
                args[i++] = gen_expr(vm, cg, arg);
            }
        }

        LLVMValueRef result = 
                LLVMBuildCall2(cg->builder, fn_type, fn, args, arg_count, "");
        free(args);
        
        if (uses_sret) {
            // Return pointer to the sret alloca (caller can read from it)
            return sret_alloca;
        }
        return result;
    }

        // =========================================================================
        // Member Access
        // =========================================================================

    case ND_MEMBER: {
        LLVMValueRef addr = gen_addr(vm, cg, node);
        LLVMTypeRef ty = llvm_type_for(cg, node->ty);
        // Don't load for arrays/structs/unions
        if (node->ty->kind == TY_ARRAY || node->ty->kind == TY_STRUCT ||
                node->ty->kind == TY_UNION) {
            return addr;
        }
        return LLVMBuildLoad2(cg->builder, ty, addr, "");
    }

        // =========================================================================
        // Comma Operator
        // =========================================================================

    case ND_COMMA: {
        gen_expr(vm, cg, node->lhs);                // Evaluate and discard
        return gen_expr(vm, cg, node->rhs); // Return rhs
    }

        // =========================================================================
        // Statement Expression
        // =========================================================================

    case ND_STMT_EXPR: {
        // Generate all statements, return value of last expression
        LLVMValueRef last_val = NULL;
        for (Node *n = node->body; n; n = n->next) {
            if (n->next == NULL && n->kind == ND_EXPR_STMT) {
                // Last statement is an expression statement - capture its value
                last_val = gen_expr(vm, cg, n->lhs);
            } else {
                gen_stmt(vm, cg, n);
            }
        }
        if (last_val)
            return last_val;
        return LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, 0);
    }

        // =========================================================================
        // Memory Zero (for local variable initialization)
        // =========================================================================

    case ND_MEMZERO: {
        // Zero out a local variable's memory using memset intrinsic
        Obj *var = node->var;
        LLVMValueRef addr = hashmap_get_int(&cg->obj_to_llvm, (long long)var);
        if (addr) {
            LLVMTypeRef i8_ty = LLVMInt8TypeInContext(cg->context);
            LLVMTypeRef i32_ty = LLVMInt32TypeInContext(cg->context);
            LLVMValueRef zero = LLVMConstInt(i8_ty, 0, 0);
            LLVMValueRef size = LLVMConstInt(i32_ty, var->ty->size, 0);
            LLVMBuildMemSet(cg->builder, addr, zero, size,
                                            var->align > 0 ? var->align : 1);
        }
        return LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, 0);
    }

        // =========================================================================
        // Frame Address Builtin
        // =========================================================================

    case ND_FRAME_ADDR: {
        // __builtin_frame_address(0) - returns current frame pointer
        // Use LLVM's llvm.frameaddress intrinsic
        LLVMTypeRef ptr_ty = LLVMPointerTypeInContext(cg->context, 0);
        LLVMTypeRef i32_ty = LLVMInt32TypeInContext(cg->context);
        LLVMTypeRef fn_ty = LLVMFunctionType(ptr_ty, &i32_ty, 1, 0);
        
        LLVMValueRef intrinsic = LLVMGetIntrinsicDeclaration(
            cg->module, 
            LLVMLookupIntrinsicID("llvm.frameaddress.p0", 20),
            &ptr_ty, 1);
        
        LLVMValueRef args[] = { LLVMConstInt(i32_ty, 0, 0) };
        return LLVMBuildCall2(cg->builder, fn_ty, intrinsic, args, 1, "");
    }

        // =========================================================================
        // Null Expression
        // =========================================================================

    case ND_NULL_EXPR:
        return LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, 0);

    default:
        // Placeholder: return zero for unimplemented expressions
        fprintf(stderr, "Warning: unimplemented expression kind %d\n", node->kind);
        return LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, 0);
    }
}

// =============================================================================
// Code Generation - Statements
// =============================================================================

static void gen_stmt(JCC *vm, LLVMCodegen *cg, Node *node) {
    if (!node)
        return;

    switch (node->kind) {
    case ND_RETURN: {
        if (node->lhs) {
            Type *ret_ty = node->lhs->ty;
            if ((ret_ty->kind == TY_STRUCT || ret_ty->kind == TY_UNION) && cg->sret_ptr) {
                // Copy aggregate to sret pointer
                LLVMValueRef src = gen_addr(vm, cg, node->lhs);
                LLVMTypeRef i64_ty = LLVMInt64TypeInContext(cg->context);
                LLVMValueRef size = LLVMConstInt(i64_ty, ret_ty->size, 0);
                unsigned align = ret_ty->align > 0 ? ret_ty->align : 1;
                LLVMBuildMemCpy(cg->builder, cg->sret_ptr, align, src, align, size);
                LLVMBuildRetVoid(cg->builder);
            } else {
                LLVMValueRef val = gen_expr(vm, cg, node->lhs);
                LLVMBuildRet(cg->builder, val);
            }
        } else {
            LLVMBuildRetVoid(cg->builder);
        }
        // Create a new unreachable block to collect any subsequent code
        LLVMBasicBlockRef unreachable = LLVMAppendBasicBlockInContext(
                cg->context, cg->current_fn, "unreachable");
        LLVMPositionBuilderAtEnd(cg->builder, unreachable);
        break;
    }

    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(vm, cg, n);
        break;

    case ND_EXPR_STMT:
        gen_expr(vm, cg, node->lhs);
        break;

    case ND_IF: {
        LLVMValueRef cond = gen_expr(vm, cg, node->cond);
        LLVMValueRef cond_bool = LLVMBuildICmp(
                cg->builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, 0), "");

        LLVMBasicBlockRef then_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "if.then");
        LLVMBasicBlockRef else_bb =
                node->els ? LLVMAppendBasicBlockInContext(cg->context, cg->current_fn,
                                                                                                    "if.else")
                                    : NULL;
        LLVMBasicBlockRef merge_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "if.end");

        if (node->els) {
            LLVMBuildCondBr(cg->builder, cond_bool, then_bb, else_bb);
        } else {
            LLVMBuildCondBr(cg->builder, cond_bool, then_bb, merge_bb);
        }

        // Then block
        LLVMPositionBuilderAtEnd(cg->builder, then_bb);
        gen_stmt(vm, cg, node->then);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
            LLVMBuildBr(cg->builder, merge_bb);
        }

        // Else block
        if (node->els) {
            LLVMPositionBuilderAtEnd(cg->builder, else_bb);
            gen_stmt(vm, cg, node->els);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
                LLVMBuildBr(cg->builder, merge_bb);
            }
        }

        LLVMPositionBuilderAtEnd(cg->builder, merge_bb);
        break;
    }

    case ND_FOR: {
        // Generate init
        if (node->init)
            gen_stmt(vm, cg, node->init);

        LLVMBasicBlockRef cond_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "for.cond");
        LLVMBasicBlockRef body_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "for.body");
        LLVMBasicBlockRef inc_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "for.inc");
        LLVMBasicBlockRef end_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "for.end");

        // Save and set break/continue targets
        LLVMBasicBlockRef old_break = cg->break_block;
        LLVMBasicBlockRef old_continue = cg->continue_block;
        cg->break_block = end_bb;
        cg->continue_block = inc_bb;

        // Register break/continue labels so ND_GOTO can find them
        if (node->brk_label) {
            hashmap_put(&cg->label_blocks, node->brk_label, end_bb);
        }
        if (node->cont_label) {
            hashmap_put(&cg->label_blocks, node->cont_label, inc_bb);
        }

        LLVMBuildBr(cg->builder, cond_bb);

        // Condition
        LLVMPositionBuilderAtEnd(cg->builder, cond_bb);
        if (node->cond) {
            LLVMValueRef cond = gen_expr(vm, cg, node->cond);
            LLVMValueRef cond_bool =
                    LLVMBuildICmp(cg->builder, LLVMIntNE, cond,
                                                LLVMConstInt(LLVMTypeOf(cond), 0, 0), "");
            LLVMBuildCondBr(cg->builder, cond_bool, body_bb, end_bb);
        } else {
            LLVMBuildBr(cg->builder, body_bb);
        }

        // Body
        LLVMPositionBuilderAtEnd(cg->builder, body_bb);
        gen_stmt(vm, cg, node->then);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
            LLVMBuildBr(cg->builder, inc_bb);
        }

        // Increment
        LLVMPositionBuilderAtEnd(cg->builder, inc_bb);
        if (node->inc)
            gen_expr(vm, cg, node->inc);
        LLVMBuildBr(cg->builder, cond_bb);

        LLVMPositionBuilderAtEnd(cg->builder, end_bb);

        // Restore break/continue targets
        cg->break_block = old_break;
        cg->continue_block = old_continue;
        break;
    }

    case ND_DO: {
        // do-while: body is executed at least once before condition check
        LLVMBasicBlockRef body_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "do.body");
        LLVMBasicBlockRef cond_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "do.cond");
        LLVMBasicBlockRef end_bb =
                LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, "do.end");

        // Save and set break/continue targets
        LLVMBasicBlockRef old_break = cg->break_block;
        LLVMBasicBlockRef old_continue = cg->continue_block;
        cg->break_block = end_bb;
        cg->continue_block = cond_bb;

        // Register break/continue labels so ND_GOTO can find them
        if (node->brk_label) {
            hashmap_put(&cg->label_blocks, node->brk_label, end_bb);
        }
        if (node->cont_label) {
            hashmap_put(&cg->label_blocks, node->cont_label, cond_bb);
        }

        // Jump to body
        LLVMBuildBr(cg->builder, body_bb);

        // Body
        LLVMPositionBuilderAtEnd(cg->builder, body_bb);
        gen_stmt(vm, cg, node->then);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
            LLVMBuildBr(cg->builder, cond_bb);
        }

        // Condition (evaluated after body)
        LLVMPositionBuilderAtEnd(cg->builder, cond_bb);
        LLVMValueRef cond = gen_expr(vm, cg, node->cond);
        LLVMValueRef cond_bool = LLVMBuildICmp(
                cg->builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, 0), "");
        LLVMBuildCondBr(cg->builder, cond_bool, body_bb, end_bb);

        LLVMPositionBuilderAtEnd(cg->builder, end_bb);

        // Restore break/continue targets
        cg->break_block = old_break;
        cg->continue_block = old_continue;
        break;
    }

    case ND_SWITCH: {
        // Evaluate switch expression
        LLVMValueRef cond = gen_expr(vm, cg, node->cond);

        LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                cg->context, cg->current_fn, "switch.end");

        // Save old break target
        LLVMBasicBlockRef old_break = cg->break_block;
        cg->break_block = end_bb;

        // Register the break label so break statements (ND_GOTO) can find the
        // target
        if (node->brk_label) {
            hashmap_put(&cg->label_blocks, node->brk_label, end_bb);
        }

        // Create default block (may be unused if no default label)
        LLVMBasicBlockRef default_bb = end_bb;

        // First pass: create basic blocks for all cases and find default
        int num_cases = 0;
        for (Node *n = node->case_next; n; n = n->case_next) {
            char name[32];
            snprintf(name, sizeof(name), "switch.case.%ld", n->begin);
            LLVMBasicBlockRef case_bb =
                    LLVMAppendBasicBlockInContext(cg->context, cg->current_fn, name);
            // Store the block in the case node's label mapping
            hashmap_put(&cg->label_blocks, n->label, case_bb);
            num_cases++;
        }

        // Handle default case
        if (node->default_case) {
            default_bb = LLVMAppendBasicBlockInContext(cg->context, cg->current_fn,
                                                                                                 "switch.default");
            hashmap_put(&cg->label_blocks, node->default_case->label, default_bb);
        }

        // Build the switch instruction
        LLVMValueRef sw = LLVMBuildSwitch(cg->builder, cond, default_bb, num_cases);

        // Add cases to switch
        for (Node *n = node->case_next; n; n = n->case_next) {
            LLVMBasicBlockRef case_bb = hashmap_get(&cg->label_blocks, n->label);
            // Handle case range (begin to end)
            for (long val = n->begin; val <= n->end; val++) {
                LLVMValueRef case_val = LLVMConstInt(LLVMTypeOf(cond), val, 0);
                LLVMAddCase(sw, case_val, case_bb);
            }
        }

        // Generate the switch body (cases will jump to their blocks via ND_CASE)
        gen_stmt(vm, cg, node->then);

        // Make sure we branch to end if no terminator
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
            LLVMBuildBr(cg->builder, end_bb);
        }

        LLVMPositionBuilderAtEnd(cg->builder, end_bb);

        // Restore break target
        cg->break_block = old_break;
        break;
    }

    case ND_CASE: {
        // Get the basic block for this case
        LLVMBasicBlockRef case_bb = hashmap_get(&cg->label_blocks, node->label);
        if (case_bb) {
            // Branch to case block if current block has no terminator
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
                LLVMBuildBr(cg->builder, case_bb);
            }
            LLVMPositionBuilderAtEnd(cg->builder, case_bb);
        }
        // Generate case body
        gen_stmt(vm, cg, node->lhs);
        break;
    }

    case ND_LABEL: {
        // Get or create basic block for this label
        LLVMBasicBlockRef label_bb =
                hashmap_get(&cg->label_blocks, node->unique_label);
        if (!label_bb) {
            label_bb = LLVMAppendBasicBlockInContext(
                    cg->context, cg->current_fn, node->label ? node->label : "label");
            hashmap_put(&cg->label_blocks, node->unique_label, label_bb);
        }

        // Branch to label block if current block has no terminator
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
            LLVMBuildBr(cg->builder, label_bb);
        }
        LLVMPositionBuilderAtEnd(cg->builder, label_bb);

        // Generate the labeled statement
        gen_stmt(vm, cg, node->lhs);
        break;
    }

    case ND_GOTO: {
        // Get or create the target basic block
        LLVMBasicBlockRef target =
                hashmap_get(&cg->label_blocks, node->unique_label);
        if (!target) {
            // Label not yet seen - create a forward reference
            target = LLVMAppendBasicBlockInContext(cg->context, cg->current_fn,
                                                                                         node->label ? node->label
                                                                                                                 : "goto.target");
            hashmap_put(&cg->label_blocks, node->unique_label, target);
        }
        LLVMBuildBr(cg->builder, target);

        // Create unreachable block for any code following goto
        LLVMBasicBlockRef unreachable = LLVMAppendBasicBlockInContext(
                cg->context, cg->current_fn, "goto.unreachable");
        LLVMPositionBuilderAtEnd(cg->builder, unreachable);
        break;
    }

    default:
        // Check if this is break/continue by looking at the brk_label/cont_label
        // These aren't separate node types in chibicc - they're handled via goto to
        // labels
        break;
    }
}

// =============================================================================
// Code Generation - Global Variables
// =============================================================================

/*!
 @function create_const_from_bytes
 @abstract Convert a byte buffer to an LLVM constant.
 @param cg The LLVMCodegen instance.
 @param data Pointer to byte data.
 @param size Size of the data in bytes.
 @return LLVM constant representing the byte data as an i8 array.
*/
static LLVMValueRef create_const_from_bytes(LLVMCodegen *cg, const char *data,
                                                                                        int size) {
    LLVMValueRef *bytes = calloc(size, sizeof(LLVMValueRef));
    LLVMTypeRef i8_type = LLVMInt8TypeInContext(cg->context);

    for (int i = 0; i < size; i++) {
        bytes[i] = LLVMConstInt(i8_type, (unsigned char)data[i], 0);
    }

    LLVMValueRef array = LLVMConstArray(i8_type, bytes, size);
    free(bytes);
    return array;
}

/*!
 @function gen_global
 @abstract Generate an LLVM global variable from an Obj.
 @param cg The LLVMCodegen instance.
 @param var The global variable object to generate.
*/
static void gen_global(LLVMCodegen *cg, Obj *var) {
    LLVMValueRef global;

    if (var->init_data) {
        // For initialized globals, use byte array representation.
        // This works for all types since we're copying the raw bytes.
        LLVMTypeRef array_ty =
                LLVMArrayType(LLVMInt8TypeInContext(cg->context), var->ty->size);
        global = LLVMAddGlobal(cg->module, array_ty, var->name);

        LLVMValueRef init =
                create_const_from_bytes(cg, var->init_data, var->ty->size);
        LLVMSetInitializer(global, init);
    } else {
        // Uninitialized: use original type with zero initializer
        LLVMTypeRef ty = llvm_type_for(cg, var->ty);
        global = LLVMAddGlobal(cg->module, ty, var->name);
        LLVMSetInitializer(global, LLVMConstNull(ty));
    }

    // Set linkage based on storage class
    if (var->is_static) {
        LLVMSetLinkage(global, LLVMInternalLinkage);
    } else if (var->is_tentative) {
        LLVMSetLinkage(global, LLVMCommonLinkage);
    } else {
        LLVMSetLinkage(global, LLVMExternalLinkage);
    }

    // Set alignment
    if (var->align > 0) {
        LLVMSetAlignment(global, var->align);
    }

    // Cache for later use in expressions
    hashmap_put(&cg->obj_to_llvm, var->name, global);
}

// =============================================================================
// Code Generation - Functions
// =============================================================================

static void gen_function_llvm(JCC *vm, LLVMCodegen *cg, Obj *fn) {
    if (!fn->is_function || !fn->is_definition)
        return;

    // Clear per-function state
    // Reset label blocks for new function (labels are function-scoped)
    memset(&cg->label_blocks, 0, sizeof(cg->label_blocks));
    cg->break_block = NULL;
    cg->continue_block = NULL;
    cg->sret_ptr = NULL;  // Reset sret pointer

    // Check if function returns struct/union (uses sret convention)
    Type *ret_ty = fn->ty->return_ty;
    bool uses_sret = (ret_ty->kind == TY_STRUCT || ret_ty->kind == TY_UNION);

    // Create function type
    LLVMTypeRef fn_type = llvm_type_for(cg, fn->ty);

    // Check if function already declared (e.g., forward declaration)
    LLVMValueRef llvm_fn = LLVMGetNamedFunction(cg->module, fn->name);
    if (!llvm_fn) {
        llvm_fn = LLVMAddFunction(cg->module, fn->name, fn_type);
    }
    cg->current_fn = llvm_fn;

    // Set calling convention (C calling convention)
    LLVMSetFunctionCallConv(llvm_fn, LLVMCCallConv);

    // Set linkage based on storage class
    if (fn->is_static) {
        LLVMSetLinkage(llvm_fn, LLVMInternalLinkage);
    }

    // Store function mapping (by name for function lookup)
    hashmap_put(&cg->obj_to_llvm, fn->name, llvm_fn);

    // Create entry basic block
    LLVMBasicBlockRef entry =
            LLVMAppendBasicBlockInContext(cg->context, llvm_fn, "entry");
    LLVMPositionBuilderAtEnd(cg->builder, entry);

    // Handle sret parameter for aggregate returns
    if (uses_sret) {
        cg->sret_ptr = LLVMGetParam(llvm_fn, 0);
        // Note: LLVM 15+ uses AddAttributeAtIndex for sret attribute
        // For now, the opaque pointer approach works without explicit sret attribute
    }

    // Allocate stack space for ALL local variables (including parameters)
    // The locals list contains all local variables including parameters
    for (Obj *var = fn->locals; var; var = var->next) {
        LLVMTypeRef var_type = llvm_type_for(cg, var->ty);
        LLVMValueRef alloca =
                LLVMBuildAlloca(cg->builder, var_type, var->name ? var->name : "");
        if (var->align > 0) {
            LLVMSetAlignment(alloca, var->align);
        }
        // Map using Obj* pointer as key to handle shadowed variables
        hashmap_put_int(&cg->obj_to_llvm, (long long)var, alloca);
    }

    // Store function parameters into their allocas
    // Parameters are stored in fn->params as a linked list
    // If using sret, param indices are shifted by 1
    int param_idx = uses_sret ? 1 : 0;
    for (Obj *param = fn->params; param; param = param->next) {
        LLVMValueRef param_alloca =
                hashmap_get_int(&cg->obj_to_llvm, (long long)param);
        if (param_alloca) {
            LLVMValueRef param_val = LLVMGetParam(llvm_fn, param_idx);
            Type *param_ty = param->ty;
            
            if (param_ty->kind == TY_STRUCT || param_ty->kind == TY_UNION) {
                // Byval parameter: copy from passed pointer to local alloca
                LLVMTypeRef i64_ty = LLVMInt64TypeInContext(cg->context);
                LLVMValueRef size = LLVMConstInt(i64_ty, param_ty->size, 0);
                unsigned align = param_ty->align > 0 ? param_ty->align : 1;
                LLVMBuildMemCpy(cg->builder, param_alloca, align, param_val, align, size);
            } else {
                // Scalar parameter: direct store
                LLVMBuildStore(cg->builder, param_val, param_alloca);
            }
        }
        param_idx++;
    }

    // Generate function body
    gen_stmt(vm, cg, fn->body);

    // Ensure function has a terminator
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(cg->builder);
    if (!LLVMGetBasicBlockTerminator(current_block)) {
        if (fn->ty->return_ty->kind == TY_VOID || uses_sret) {
            LLVMBuildRetVoid(cg->builder);
        } else {
            // Create appropriate zero value based on return type
            LLVMTypeRef ret_llvm_ty = llvm_type_for(cg, fn->ty->return_ty);
            LLVMValueRef zero;
            if (fn->ty->return_ty->kind == TY_FLOAT || 
                fn->ty->return_ty->kind == TY_DOUBLE ||
                fn->ty->return_ty->kind == TY_LDOUBLE) {
                zero = LLVMConstReal(ret_llvm_ty, 0.0);
            } else {
                zero = LLVMConstInt(ret_llvm_ty, 0, 0);
            }
            LLVMBuildRet(cg->builder, zero);
        }
    }
}

// =============================================================================
// Main Compilation Entry Point
// =============================================================================

int cc_compile_llvm(JCC *vm, LLVMCodegen *cg, Obj *prog) {
    fprintf(stderr, "[LLVM DEBUG] Starting cc_compile_llvm\n");
    
    // First pass: declare ALL functions (both declarations and definitions)
    // This allows functions to call each other regardless of definition order
    fprintf(stderr, "[LLVM DEBUG] Pass 1: declaring functions...\n");
    for (Obj *obj = prog; obj; obj = obj->next) {
        if (obj->is_function) {
            fprintf(stderr, "[LLVM DEBUG]   Declaring function: %s\n", obj->name);
            LLVMTypeRef fn_type = llvm_type_for(cg, obj->ty);
            LLVMValueRef fn = LLVMAddFunction(cg->module, obj->name, fn_type);
            hashmap_put(&cg->obj_to_llvm, obj->name, fn);
        }
    }

    // Second pass: generate globals
    fprintf(stderr, "[LLVM DEBUG] Pass 2: generating globals...\n");
    for (Obj *obj = prog; obj; obj = obj->next) {
        if (!obj->is_function) {
            fprintf(stderr, "[LLVM DEBUG]   Generating global: %s\n", obj->name);
            gen_global(cg, obj);
        }
    }

    // Third pass: generate function definitions (bodies)
    fprintf(stderr, "[LLVM DEBUG] Pass 3: generating function bodies...\n");
    for (Obj *obj = prog; obj; obj = obj->next) {
        if (obj->is_function && obj->is_definition) {
            fprintf(stderr, "[LLVM DEBUG]   Generating body for: %s\n", obj->name);
            gen_function_llvm(vm, cg, obj);
        }
    }

    // Verify the module
    fprintf(stderr, "[LLVM DEBUG] Verifying module...\n");
    char *error = NULL;
    if (LLVMVerifyModule(cg->module, LLVMReturnStatusAction, &error)) {
        fprintf(stderr, "LLVM module verification failed: %s\n", error);
        LLVMDisposeMessage(error);
        return -1;
    }

    fprintf(stderr, "[LLVM DEBUG] cc_compile_llvm completed successfully\n");
    return 0;
}

// =============================================================================
// AOT Compilation - Emit Object File
// =============================================================================

/*!
 @function cc_emit_object_llvm
 @abstract Compile the LLVM IR module to a native object file.
 @param cg The LLVMCodegen instance containing the module.
 @param output_path Path for the output object file.
 @return 0 on success, -1 on error.
*/
int cc_emit_object_llvm(LLVMCodegen *cg, const char *output_path) {
    char *error = NULL;

    // Get the target triple for the host machine
    char *target_triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(cg->module, target_triple);

    // Look up the target
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(target_triple, &target, &error)) {
        fprintf(stderr, "Failed to get target: %s\n", error);
        LLVMDisposeMessage(error);
        LLVMDisposeMessage(target_triple);
        return -1;
    }

    // Get host CPU and features
    char *cpu = LLVMGetHostCPUName();
    char *features = LLVMGetHostCPUFeatures();

    // Create target machine
    LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(
        target,
        target_triple,
        cpu,
        features,
        LLVMCodeGenLevelDefault,
        LLVMRelocPIC,
        LLVMCodeModelDefault
    );

    if (!target_machine) {
        fprintf(stderr, "Failed to create target machine\n");
        LLVMDisposeMessage(target_triple);
        LLVMDisposeMessage(cpu);
        LLVMDisposeMessage(features);
        return -1;
    }

    // Set data layout from target machine
    LLVMTargetDataRef data_layout = LLVMCreateTargetDataLayout(target_machine);
    char *data_layout_str = LLVMCopyStringRepOfTargetData(data_layout);
    LLVMSetDataLayout(cg->module, data_layout_str);
    LLVMDisposeMessage(data_layout_str);
    LLVMDisposeTargetData(data_layout);

    // Emit object file
    if (LLVMTargetMachineEmitToFile(target_machine, cg->module, 
                                     (char *)output_path, 
                                     LLVMObjectFile, &error)) {
        fprintf(stderr, "Failed to emit object file: %s\n", error);
        LLVMDisposeMessage(error);
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeMessage(target_triple);
        LLVMDisposeMessage(cpu);
        LLVMDisposeMessage(features);
        return -1;
    }

    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeMessage(target_triple);
    LLVMDisposeMessage(cpu);
    LLVMDisposeMessage(features);

    return 0;
}

/*!
 @function cc_emit_executable_llvm
 @abstract Compile the LLVM IR module to a native executable.
 @param cg The LLVMCodegen instance containing the module.
 @param output_path Path for the output executable.
 @return 0 on success, -1 on error.
 @discussion This emits an object file and then invokes the system linker.
*/
int cc_emit_executable_llvm(LLVMCodegen *cg, const char *output_path) {
    // Generate a temporary object file name
    char obj_path[256];
    snprintf(obj_path, sizeof(obj_path), "%s.o", output_path);

    // Emit object file
    if (cc_emit_object_llvm(cg, obj_path) != 0) {
        return -1;
    }

    // Invoke system linker to create executable
    // Use cc/clang as the linker driver for simplicity
    char cmd[1024];
#if defined(__APPLE__)
    snprintf(cmd, sizeof(cmd), "cc -o '%s' '%s' -lSystem 2>&1", output_path, obj_path);
#elif defined(__linux__)
    snprintf(cmd, sizeof(cmd), "cc -o '%s' '%s' -lc 2>&1", output_path, obj_path);
#else
    snprintf(cmd, sizeof(cmd), "cc -o '%s' '%s' 2>&1", output_path, obj_path);
#endif

    int ret = system(cmd);
    
    // Clean up temporary object file
    remove(obj_path);

    if (ret != 0) {
        fprintf(stderr, "Linking failed with exit code %d\n", ret);
        return -1;
    }

    return 0;
}

// =============================================================================
// Debugging Utilities
// =============================================================================

void codegen_llvm_dump_ir(LLVMCodegen *cg) {
    if (cg->module) {
        char *ir = LLVMPrintModuleToString(cg->module);
        printf("%s\n", ir);
        LLVMDisposeMessage(ir);
    }
}

#endif /* JCC_HAS_LLVM */