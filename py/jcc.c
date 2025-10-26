/*
 PyJCC: JIT C Compiler for Python

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

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "jcc.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

static void capsule_dtor(PyObject *capsule) {
    JCC *vm = (JCC *)PyCapsule_GetPointer(capsule, "jcc.JCC");
    if (!vm)
        return;
    cc_destroy(vm);
    free(vm);
}

static JCC *get_vm_from_capsule(PyObject *capsule) {
    if (!PyCapsule_CheckExact(capsule)) {
        PyErr_SetString(PyExc_TypeError, "expected a jcc.JCC capsule");
        return NULL;
    }
    JCC *vm = (JCC *)PyCapsule_GetPointer(capsule, "jcc.JCC");
    if (!vm) {
        PyErr_SetString(PyExc_ValueError, "invalid or closed JCC capsule");
        return NULL;
    }
    return vm;
}

static PyObject *py_jcc_create(PyObject *self, PyObject *args, PyObject *kwds) {
    int enable_debugger = 0;
    static char *kwlist[] = {"enable_debugger", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|p", kwlist, &enable_debugger))
        return NULL;

    JCC *vm = calloc(1, sizeof(JCC));
    if (!vm)
        return PyErr_NoMemory();
    cc_init(vm, enable_debugger);

    PyObject *capsule = PyCapsule_New(vm, "jcc.JCC", capsule_dtor);
    if (!capsule) {
        cc_destroy(vm);
        free(vm);
        return NULL;
    }
    return capsule;
}

static PyObject *py_jcc_destroy(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    if (!PyArg_ParseTuple(args, "O", &capsule))
        return NULL;

    if (!PyCapsule_CheckExact(capsule)) {
        PyErr_SetString(PyExc_TypeError, "expected a jcc.JCC capsule");
        return NULL;
    }

    JCC *vm = (JCC *)PyCapsule_GetPointer(capsule, "jcc.JCC");
    if (vm == NULL) {
        if (PyErr_Occurred())
            return NULL;
        Py_RETURN_NONE;
    }

    /* Disable the capsule destructor first so it won't run later. */
    PyCapsule_SetDestructor(capsule, NULL);

    cc_destroy(vm);
    free(vm);

    Py_RETURN_NONE;
}

static PyObject *py_jcc_load_stdlib(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    if (!PyArg_ParseTuple(args, "O", &capsule))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm)
        return NULL;

    cc_load_stdlib(vm);
    Py_RETURN_NONE;
}

static PyObject *py_jcc_compile_file(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "Os", &capsule, &path))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm)
        return NULL;

    Token *tok = cc_preprocess(vm, path);
    if (!tok) {
        PyErr_Format(PyExc_RuntimeError, "preprocessing failed for '%s'", path);
        return NULL;
    }
    Obj *prog = cc_parse(vm, tok);
    if (!prog) {
        PyErr_Format(PyExc_RuntimeError, "parsing failed for '%s'", path);
        return NULL;
    }

    cc_compile(vm, prog);

    Py_RETURN_TRUE;
}

static PyObject *py_jcc_include(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "Os", &capsule, &path))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm)
        return NULL;

    cc_include(vm, path);
    Py_RETURN_NONE;
}

static PyObject *py_jcc_define(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    const char *name = NULL;
    const char *buf = NULL;
    if (!PyArg_ParseTuple(args, "Oss", &capsule, &name, &buf))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm)
        return NULL;

    /* cc_define expects mutable char*, duplicate to be safe */
    cc_define(vm, strdup(name), strdup(buf));

    Py_RETURN_NONE;
}

static PyObject *py_jcc_undef(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    const char *name = NULL;
    if (!PyArg_ParseTuple(args, "Os", &capsule, &name))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm)
        return NULL;

    cc_undef(vm, strdup(name));

    Py_RETURN_NONE;
}

static PyObject *py_jcc_register_cfunc(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    const char *name = NULL;
    int num_args = 0;
    int returns_double = 0;
    if (!PyArg_ParseTuple(args, "Osii", &capsule, &name, &num_args, &returns_double))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm)
        return NULL;

    void *sym = dlsym(RTLD_DEFAULT, name);
    if (!sym) {
        PyErr_Format(PyExc_RuntimeError, "native symbol '%s' not found (dlsym)", name);
        return NULL;
    }

    cc_register_cfunc(vm, name, sym, num_args, returns_double);
    Py_RETURN_NONE;
}

static PyObject *py_jcc_load_bytecode(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "Os", &capsule, &path))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm) return NULL;

    int rc = cc_load_bytecode(vm, path);
    if (rc != 0) {
        PyErr_Format(PyExc_RuntimeError, "cc_load_bytecode failed for '%s' (rc=%d)", path, rc);
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *py_jcc_print_tokens(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "Os", &capsule, &path))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm) return NULL;

    Token *tok = cc_preprocess(vm, path);
    if (!tok) {
        PyErr_Format(PyExc_RuntimeError, "preprocessing failed for '%s'", path);
        return NULL;
    }
    cc_print_tokens(tok);
    Py_RETURN_NONE;
}

static PyObject *py_jcc_add_breakpoint(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    long long offset = 0;
    if (!PyArg_ParseTuple(args, "OL", &capsule, &offset))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm) return NULL;

    long long *pc = NULL;
    if (offset < 0 || vm->text_seg == NULL) {
        PyErr_SetString(PyExc_ValueError, "invalid offset or text segment not initialized");
        return NULL;
    }
    long long text_len = (long long)(vm->text_ptr - vm->text_seg);
    if (offset >= text_len) {
        PyErr_Format(PyExc_ValueError, "offset %lld out of range (text length %lld)", offset, text_len);
        return NULL;
    }
    pc = vm->text_seg + offset;
    int idx = cc_add_breakpoint(vm, pc);
    if (idx < 0) {
        PyErr_SetString(PyExc_RuntimeError, "failed to add breakpoint");
        return NULL;
    }
    return PyLong_FromLong(idx);
}

static PyObject *py_jcc_remove_breakpoint(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    int idx = 0;
    if (!PyArg_ParseTuple(args, "Oi", &capsule, &idx))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm) return NULL;

    cc_remove_breakpoint(vm, idx);
    Py_RETURN_NONE;
}

static PyObject *py_jcc_run(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    PyObject *py_argv = NULL; /* sequence of strings */
    if (!PyArg_ParseTuple(args, "OO", &capsule, &py_argv))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm)
        return NULL;

    if (!PySequence_Check(py_argv)) {
        PyErr_SetString(PyExc_TypeError, "argv must be a sequence of strings");
        return NULL;
    }

    Py_ssize_t n = PySequence_Size(py_argv);
    if (n < 0)
        return NULL; /* error */

    /* Build an argv array of char* (NUL-terminated) */
    char **argv = calloc((size_t)n + 1, sizeof(char *));
    if (!argv)
        return PyErr_NoMemory();
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *item = PySequence_GetItem(py_argv, i); /* new ref */
        if (!item) {
            for (Py_ssize_t j = 0; j < i; j++)
                free(argv[j]);
            free(argv);
            return NULL;
        }
        const char *s = PyUnicode_AsUTF8(item);
        if (!s) {
            Py_DECREF(item);
            for (Py_ssize_t j = 0; j < i; j++)
                free(argv[j]);
            free(argv);
            return NULL;
        }
        argv[i] = strdup(s);
        Py_DECREF(item);
        if (!argv[i]) {
            for (Py_ssize_t j = 0; j < i; j++)
                free(argv[j]);
            free(argv);
            return PyErr_NoMemory();
        }
    }

    int rc = cc_run(vm, (int)n, argv);

    for (Py_ssize_t i = 0; i < n; i++)
        free(argv[i]);
    free(argv);

    return PyLong_FromLong(rc);
}

static PyObject *py_jcc_save_bytecode(PyObject *self, PyObject *args) {
    PyObject *capsule = NULL;
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "Os", &capsule, &path))
        return NULL;
    JCC *vm = get_vm_from_capsule(capsule);
    if (!vm)
        return NULL;

    int rc = cc_save_bytecode(vm, path);
    if (rc != 0) {
        PyErr_Format(PyExc_RuntimeError, "cc_save_bytecode failed for '%s' (rc=%d)", path, rc);
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Module method table */
static PyMethodDef PyJCCMethods[] = {
    {"create", (PyCFunction)py_jcc_create, METH_VARARGS | METH_KEYWORDS, "Create a new JCC instance. Returns a capsule."},
    {"destroy", py_jcc_destroy, METH_VARARGS, "Destroy a JCC instance (capsule)."},
    {"load_stdlib", py_jcc_load_stdlib, METH_VARARGS, "Load builtin stdlib FFI functions into the JCC instance."},
    {"compile_file", py_jcc_compile_file, METH_VARARGS, "Preprocess/parse/compile a C source file into the VM text segment."},
    {"run", py_jcc_run, METH_VARARGS, "Run the compiled program with argv (list of strings). Returns exit code."},
    {"save_bytecode", py_jcc_save_bytecode, METH_VARARGS, "Save VM bytecode to file. Raises on failure."},
    {"include", py_jcc_include, METH_VARARGS, "Add a directory to the include search path."},
    {"define", py_jcc_define, METH_VARARGS, "Define a preprocessor macro: name, replacement string."},
    {"undef", py_jcc_undef, METH_VARARGS, "Undefine a preprocessor macro."},
    {"register_cfunc", py_jcc_register_cfunc, METH_VARARGS, "Register a native C function by symbol name: (name, num_args, returns_double)."},
    {"load_bytecode", py_jcc_load_bytecode, METH_VARARGS, "Load VM bytecode from file (replaces current program)."},
    {"print_tokens", py_jcc_print_tokens, METH_VARARGS, "Preprocess a file and print tokens to stdout."},
    {"add_breakpoint", py_jcc_add_breakpoint, METH_VARARGS, "Add breakpoint at text-segment offset. Returns breakpoint index."},
    {"remove_breakpoint", py_jcc_remove_breakpoint, METH_VARARGS, "Remove breakpoint by index."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pyjccmodule = {
    PyModuleDef_HEAD_INIT,
    "PyJCC",
    "Python bindings for JCC",
    -1,
    PyJCCMethods
};

PyMODINIT_FUNC PyInit_PyJCC(void) {
    return PyModule_Create(&pyjccmodule);
}
