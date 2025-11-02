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

 This file was original part of chibicc by Rui Ueyama (MIT) https://github.com/rui314/chibicc
*/

#pragma once
#include "jcc.h"
#ifndef __has_include
#define __has_include(x) 0
#endif
#if __has_include(<stdnoreturn.h>)
#include <stdnoreturn.h>
#else
#define noreturn
#endif
#ifndef __attribute__
#define __attribute__(x)
#endif

#define JCC_MAGIC "JJCC"
#define JCC_VERSION 1

void strarray_push(StringArray *arr, char *s);
char *format(char *fmt, ...) __attribute__((format(printf, 1, 2)));
Token *preprocess(JCC *vm, Token *tok);

//
// tokenize.c
//

noreturn void error(char *fmt, ...) __attribute__((format(printf, 1, 2)));
noreturn void error_at(JCC *vm, char *loc, char *fmt, ...) __attribute__((format(printf, 3, 4)));
noreturn void error_tok(JCC *vm, Token *tok, char *fmt, ...) __attribute__((format(printf, 3, 4)));
void warn_tok(JCC *vm, Token *tok, char *fmt, ...) __attribute__((format(printf, 3, 4)));
bool equal(Token *tok, char *op);
Token *skip(JCC *vm, Token *tok, char *op);
bool consume(JCC *vm, Token **rest, Token *tok, char *str);
void convert_pp_tokens(JCC *vm, Token *tok);
File *new_file(char *name, int file_no, char *contents);
Token *tokenize_string_literal(JCC *vm, Token *tok, Type *basety);
Token *tokenize(JCC *vm, File *file);
Token *tokenize_file(JCC *vm, char *filename);
void cc_output_preprocessed(FILE *f, Token *tok);

#define unreachable() \
    error("internal error at %s:%d", __FILE__, __LINE__)

//
// preprocess.c
//

char *search_include_paths(JCC *vm, char *filename);
void init_macros(JCC *vm);
void define_macro(JCC *vm, char *name, char *buf);
void undef_macro(JCC *vm, char *name);
Token *preprocess(JCC *vm, Token *tok);

//
// parse.c
//

Node *new_cast(JCC *vm, Node *expr, Type *ty);
int64_t const_expr(JCC *vm, Token **rest, Token *tok);
Obj *parse(JCC *vm, Token *tok);

//
// type.c
//

extern Type *ty_void;
extern Type *ty_bool;

extern Type *ty_char;
extern Type *ty_short;
extern Type *ty_int;
extern Type *ty_long;

extern Type *ty_uchar;
extern Type *ty_ushort;
extern Type *ty_uint;
extern Type *ty_ulong;

extern Type *ty_float;
extern Type *ty_double;
extern Type *ty_ldouble;

bool is_integer(Type *ty);
bool is_flonum(Type *ty);
bool is_numeric(Type *ty);
bool is_compatible(Type *t1, Type *t2);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *array_of(Type *base, int size);
Type *vla_of(Type *base, Node *expr);
Type *enum_type(void);
Type *struct_type(void);
Type *union_type(void);
void add_type(JCC *vm, Node *node);

//
// unicode.c
//

int encode_utf8(char *buf, uint32_t c);
uint32_t decode_utf8(JCC *vm, char **new_pos, char *p);
bool is_ident1(uint32_t c);
bool is_ident2(uint32_t c);
int display_width(JCC *vm, char *p, int len);

//
// hashmap.c
//

void *hashmap_get(HashMap *map, char *key);
void *hashmap_get2(HashMap *map, char *key, int keylen);
void hashmap_put(HashMap *map, char *key, void *val);
void hashmap_put2(HashMap *map, char *key, int keylen, void *val);
void hashmap_delete(HashMap *map, char *key);
void hashmap_delete2(HashMap *map, char *key, int keylen);
void hashmap_test(void);

// HashMap iteration
// Callback function type for iteration
// Return 0 to continue iteration, non-zero to stop
typedef int (*HashMapIterator)(char *key, int keylen, void *val, void *user_data);

void hashmap_foreach(HashMap *map, HashMapIterator iter, void *user_data);
int hashmap_count_if(HashMap *map, HashMapIterator predicate, void *user_data);
void hashmap_test_iteration(void);

//
// stdlib.c
//

void register_variadic_wrappers(JCC *vm);
long long wrap_strlen(long long s);
long long wrap_strcmp(long long s1, long long s2);
long long wrap_strncmp(long long s1, long long s2, long long n);
long long wrap_memcmp(long long s1, long long s2, long long n);
long long wrap_fread(long long ptr, long long size, long long nmemb, long long stream);
long long wrap_fwrite(long long ptr, long long size, long long nmemb, long long stream);
long long wrap_read(long long fd, long long buf, long long count);
long long wrap_write(long long fd, long long buf, long long count);

//
// main.c
//

void codegen(JCC *vm, Obj *prog);

//
// codegen.c
//

void gen_function(JCC *vm, Obj *fn);
void gen_expr(JCC *vm, Node *node);

//
// vm.c
//

int vm_eval(JCC *vm);

//
// debugger.c
//

void debugger_init(JCC *vm);
void cc_debug_repl(JCC *vm);
void debugger_disassemble_current(JCC *vm);
void debugger_print_stack(JCC *vm, int count);
int debugger_check_breakpoint(JCC *vm);
void debugger_print_registers(JCC *vm);
void debugger_print_stack(JCC *vm, int count);
void debugger_disassemble_current(JCC *vm);
int debugger_run(JCC *vm, int argc, char **argv);

//
// pragma.c (pragma macro system)
//

typedef struct PragmaMacro PragmaMacro;
struct PragmaMacro {
    char *name;              // Function name
    Token *body_tokens;      // Original token stream for function body
    void *compiled_fn;       // Compiled function pointer (returns Node*)
    JCC *macro_vm;           // VM instance for this macro
    PragmaMacro *next;       // Next macro in list
};

void compile_pragma_macros(JCC *vm);
PragmaMacro *find_pragma_macro(JCC *vm, const char *name);
Node *execute_pragma_macro(JCC *vm, PragmaMacro *pm, Node **args, int arg_count);
Token *expand_pragma_macro_calls(JCC *vm, Token *tok);

//
// serialize.c (AST to source serialization)
//

char *serialize_node_to_source(JCC *vm, Node *node);