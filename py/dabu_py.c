#define PY_SSIZE_T_CLEAN
#include <stdbool.h>
#include <Python.h>

#include "../dabu.h"

static PyObject* dabu_dump(PyObject* self, PyObject* args) {
    const char* path = NULL;
    bool dump = false;

    if (!PyArg_ParseTuple(args, "si", &path, &dump)) {
        return NULL;
    }

	return PyBool_FromLong(assemblies_dump(path, dump));
}

static PyMethodDef methods[] = {
    {"dump", dabu_dump, METH_VARARGS, "dump dlls from assemblies.blob file."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "dabu",
    "A simple fast math module in C.",
    -1,
    methods
};

PyMODINIT_FUNC PyInit_dabu(void) {
    return PyModule_Create(&module);
}

