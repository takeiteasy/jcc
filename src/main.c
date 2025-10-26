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
#include <getopt.h>

static void usage(const char *argv0, int exit_code) {
    printf("JCC: JIT C Compiler\n");
    printf("https://github.com/takeiteasy/jcc\n\n");
    printf("Usage: %s [options] file...\n\n", argv0);
    printf("Options:\n");
    printf("\t-h/--help           Show this message\n");
    printf("\t-I <path>           Add <path> to include search paths\n");
    printf("\t-D <macro>[=def]    Define a macro\n");
    printf("\t-U <macro>          Undefine a macro\n");
    printf("\t-a/--ast            Dump AST (TODO)\n");
    printf("\t-P/--print-tokens   Print preprocessed tokens to stdout\n");
    printf("\t-X/--no-preprocess  Disable preprocessing step\n");
    printf("\t-S/--no-stdlib      Do not link standard library\n");
    printf("\t-o/--out <file>     Dump bytecode to <file> (no execution)\n");
    printf("\t-v/--verbose        Enable debug logging\n");
    printf("\t-g/--debug          Enable interactive debugger\n");
    printf("\nMemory Safety Options:\n");
    printf("\t-b/--bounds-checks           Runtime array bounds checking\n");
    printf("\t-f/--uaf-detection           Use-after-free detection\n");
    printf("\t-t/--type-checks             Runtime type checking on pointer dereferences\n");
    printf("\t-z/--uninitialized-detection Uninitialized variable detection\n");
    printf("\t-s/--stack-canaries          Stack overflow protection\n");
    printf("\t-k/--heap-canaries           Heap overflow protection\n");
    printf("\t-p/--pointer-sanitizer       Full pointer tracking and validation\n");
    printf("\t-l/--memory-leak-detection   Track allocations and report leaks at exit\n");
    printf("\t-i/--stack-instrumentation   Track stack variable lifetimes and accesses\n");
    printf("\nExample:\n");
    printf("\t%s -o hello hello.c\n", argv0);
    printf("\t%s -I ./include -D DEBUG -o prog prog.c\n", argv0);
    printf("\techo 'int main() { return 42; }' | %s -\n", argv0);
    printf("\n");
    exit(exit_code);
}

static char *read_stdin_to_tmp(void) {
#if defined(_WIN32)
    char tmpPath[MAX_PATH + 1];
    char tmpFile[MAX_PATH + 1];
    DWORD len = GetTempPathA(MAX_PATH, tmpPath);
    if (len == 0 || len > MAX_PATH)
        return NULL;
    if (GetTempFileNameA(tmpPath, "asi", 0, tmpFile) == 0)
        return NULL;
    HANDLE h = CreateFileA(tmpFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        DWORD written = 0;
        if (!WriteFile(h, buf, (DWORD)n, &written, NULL) || written != (DWORD)n) {
            CloseHandle(h);
            DeleteFileA(tmpFile);
            return NULL;
        }
    }
    if (ferror(stdin)) {
        CloseHandle(h);
        DeleteFileA(tmpFile);
        return NULL;
    }
    CloseHandle(h);
    return _strdup(tmpFile);
#else
    char template[] = "/tmp/jcc-stdin-XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0)
        return NULL;
    char buf[4096];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        ssize_t w = 0;
        while (w < n) {
            ssize_t m = write(fd, buf + w, n - w);
            if (m < 0) { close(fd); unlink(template); return NULL; }
            w += m;
        }
    }
    if (n < 0) {
        close(fd);
        unlink(template);
        return NULL;
    }
    if (close(fd) < 0) {
        unlink(template);
        return NULL;
    }
    return strdup(template);
#endif
}

static void parse_define(JCC *vm, char *arg) {
    char *eq = strchr(arg, '=');
    if (eq) {
        *eq = '\0';
        cc_define(vm, arg, eq + 1);
    } else
        cc_define(vm, arg, "1");
}


int main(int argc, const char* argv[]) {
    int exit_code = 0;
    const char **input_files = NULL;
    int input_files_count = 0;
    const char **inc_paths = NULL; // -I
    int inc_paths_count = 0;
    const char **defines = NULL; // -D
    int defines_count = 0;
    const char **undefs = NULL; // -U
    int undefs_count = 0;
    char *out_file = NULL; // -o (single)
    int dump_ast = 0;      // -a
    int verbose = 0;       // -v
    int enable_debugger = 0; // --debug / -g
    int enable_bounds_checks = 0;
    int enable_uaf_detection = 0;
    int enable_type_checks = 0;
    int enable_uninitialized_detection = 0;
    int enable_stack_canaries = 0;
    int enable_heap_canaries = 0;
    int enable_pointer_sanitizer = 0;
    int enable_memory_leak_detection = 0;
    int enable_stack_instrumentation = 0;
    int print_tokens = 0; // -P
    int skip_preprocess = 0; // -X
    int skip_stdlib = 0; // -S

    if (argc <= 1)
        usage(argv[0], 1);

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"out", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"ast", no_argument, 0, 'a'},
        {"print-tokens", no_argument, 0, 'P'},
        {"no-preprocess", no_argument, 0, 'X'},
        {"no-stdlib", no_argument, 0, 'S'},
        {"debug", no_argument, 0, 'g'},
        {"enable-bounds-checks", no_argument, 0, 'b'},
        {"enable-uaf-detection", no_argument, 0, 'f'},
        {"enable-type-checks", no_argument, 0, 't'},
        {"enable-uninitialized-detection", no_argument, 0, 'z'},
        {"enable-stack-canaries", no_argument, 0, 's'},
        {"enable-heap-canaries", no_argument, 0, 'k'},
        {"enable-pointer-sanitizer", no_argument, 0, 'p'},
        {"enable-memory-leak-detection", no_argument, 0, 'l'},
        {"enable-stack-instrumentation", no_argument, 0, 'i'},
        {"include", required_argument, 0, 'I'},
        {"define", required_argument, 0, 'D'},
        {"undef", required_argument, 0, 'U'},
        {0, 0, 0, 0}
    };

    const char *optstring = "haI:D:U:o:vgbftzskpliPX";
    int opt;
    opterr = 0; // we'll handle errors explicitly
    while ((opt = getopt_long(argc, (char * const *)argv, optstring, long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0], 0);
            break;
        case 'o':
            if (out_file) { fprintf(stderr, "error: only one -o/--out allowed\n"); usage(argv[0], 1); }
            out_file = strdup(optarg);
            break;
        case 'v':
            verbose = 1;
            break;
        case 'a':
            dump_ast = 1;
            break;
        case 'g':
            enable_debugger = 1;
            break;
        case 'I':
            inc_paths = realloc(inc_paths, sizeof(*inc_paths) * (inc_paths_count + 1));
            inc_paths[inc_paths_count++] = strdup(optarg);
            break;
        case 'D':
            defines = realloc(defines, sizeof(*defines) * (defines_count + 1));
            defines[defines_count++] = strdup(optarg);
            break;
        case 'U':
            undefs = realloc(undefs, sizeof(*undefs) * (undefs_count + 1));
            undefs[undefs_count++] = strdup(optarg);
            break;
        case 'b':
            enable_bounds_checks = 1;
            break;
        case 'f':
            enable_uaf_detection = 1;
            break;
        case 't':
            enable_type_checks = 1;
            break;
        case 'z':
            enable_uninitialized_detection = 1;
            break;
        case 's':
            enable_stack_canaries = 1;
            break;
        case 'k':
            enable_heap_canaries = 1;
            break;
        case 'p':
            enable_pointer_sanitizer = 1;
            break;
        case 'l':
            enable_memory_leak_detection = 1;
            break;
        case 'i':
            enable_stack_instrumentation = 1;
            break;
        case 'P':
            print_tokens = 1;
            break;
        case 'X':
            skip_preprocess = 1;
            break;
        case '?':
            if (optopt)
                fprintf(stderr, "error: option -%c requires an argument\n", optopt);
            else if (optind > 0 && argv[optind-1] && argv[optind-1][0] == '-')
                fprintf(stderr, "error: unknown option %s\n", argv[optind-1]);
            else
                fprintf(stderr, "error: unknown parsing error\n");
            usage(argv[0], 1);
            break;
        default:
            usage(argv[0], 1);
        }
    }

    /* Remaining arguments are input files (positional) */
    for (int i = optind; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-") == 0) {
            input_files = realloc(input_files, sizeof(*input_files) * (input_files_count + 1));
            input_files[input_files_count++] = strdup("-");
        } else {
            input_files = realloc(input_files, sizeof(*input_files) * (input_files_count + 1));
            input_files[input_files_count++] = strdup(a);
        }
    }
    // If no input files, error
    if (input_files_count == 0) {
        fprintf(stderr, "error: no input files\n");
        usage((char *)argv[0], 1);
    }

    // If the only input file is "-", read stdin into a temporary file and replace it
    if (input_files_count == 1 && strcmp(input_files[0], "-") == 0) {
        char *tmp = read_stdin_to_tmp();
        if (!tmp) {
            fprintf(stderr, "error: failed to read stdin into temporary file\n");
            // cleanup before exit
            for (int i = 0; i < inc_paths_count; i++)
                free((void *)inc_paths[i]);
            free(inc_paths);
            for (int i = 0; i < defines_count; i++)
                free((void *)defines[i]);
            free(defines);
            for (int i = 0; i < undefs_count; i++)
                free((void *)undefs[i]);
            free(undefs);
            for (int i = 0; i < input_files_count; i++)
                free((void *)input_files[i]);
            free(input_files);
            free(out_file);
            usage((char *)argv[0], 1);
        }
        free((void *)input_files[0]);
        input_files[0] = tmp;
    }

    JCC vm;
    cc_init(&vm, enable_debugger);

    if (verbose)
        vm.debug_vm = 1;

    vm.enable_bounds_checks = enable_bounds_checks;
    vm.enable_uaf_detection = enable_uaf_detection;
    vm.enable_type_checks = enable_type_checks;
    vm.enable_uninitialized_detection = enable_uninitialized_detection;
    vm.enable_stack_canaries = enable_stack_canaries;
    vm.enable_heap_canaries = enable_heap_canaries;
    vm.enable_pointer_sanitizer = enable_pointer_sanitizer;
    vm.enable_memory_leak_detection = enable_memory_leak_detection;
    vm.enable_stack_instrumentation = enable_stack_instrumentation;

    if (!skip_stdlib)
        cc_load_stdlib(&vm);
    for (int i = 0; i < inc_paths_count; i++)
        cc_include(&vm, inc_paths[i]);
    for (int i = 0; i < defines_count; i++)
        parse_define(&vm, (char *)defines[i]);
    for (int i = 0; i < undefs_count; i++)
        cc_undef(&vm, (char *)undefs[i]);

    vm.skip_preprocess = skip_preprocess;
    Obj **input_progs = NULL;
    Token **input_tokens = calloc(input_files_count, sizeof(Token*));
    for (int i = 0; i < input_files_count; i++) {
        input_tokens[i] = cc_preprocess(&vm, input_files[i]);
        if (!input_tokens[i]) {
            fprintf(stderr, "error: failed to preprocess %s\n", input_files[i]);
            goto BAIL;
        }
    }
    
    input_progs = calloc(input_files_count, sizeof(Obj*));
    for (int i = 0; i < input_files_count; i++) {
        input_progs[i] = cc_parse(&vm, input_tokens[i]);
        if (!input_progs[i]) {
            fprintf(stderr, "error: failed to parse %s\n", input_files[i]);
            goto BAIL;
        }
    }

    // Link all programs together
    Obj *merged_prog = cc_link_progs(&vm, input_progs, input_files_count);
    if (!merged_prog) {
        fprintf(stderr, "error: failed to link programs\n");
        goto BAIL;
    }

    if (print_tokens) {
        for (int i = 0; i < input_files_count; i++) {
            printf("=== Tokens for %s ===\n", input_files[i]);
            cc_print_tokens(input_tokens[i]);
            printf("\n");
        }
        goto BAIL;
    }

    if (dump_ast) {
        // TODO: Implement AST dumping
        fprintf(stderr, "warning: -e/--dump-ast not yet implemented\n");
        goto BAIL;
    }

    // Compile the merged program
    cc_compile(&vm, merged_prog);

    if (out_file) {
        // Save bytecode to file and exit
        if (cc_save_bytecode(&vm, out_file) != 0) {
            fprintf(stderr, "error: failed to save bytecode to %s\n", out_file);
            exit_code = 1;
            goto BAIL;
        }
        printf("Bytecode saved to %s\n", out_file);
        goto BAIL;
    }

    // Run the program
    exit_code = cc_run(&vm, argc, (char**)argv);
    
BAIL:
    cc_destroy(&vm);
    if (input_tokens)
        free(input_tokens);
    if (input_progs) {
        for (int i = 0; i < input_files_count; i++)
            free((void *)input_progs[i]);
        free(input_progs);
    }
    if (out_file)
        free(out_file);
    if (inc_paths) {
        for (int i = 0; i < inc_paths_count; i++)
            free((void *)inc_paths[i]);
        free(inc_paths);
    }
    if (defines) {
        for (int i = 0; i < defines_count; i++)
            free((void *)defines[i]);
        free(defines);
    }
    if (undefs) {
        for (int i = 0; i < undefs_count; i++)
            free((void *)undefs[i]);
        free(undefs);
    }
    if (input_files) {
        for (int i = 0; i < input_files_count; i++)
            free((void *)input_files[i]);
        free(input_files);
    }
    return exit_code;
}