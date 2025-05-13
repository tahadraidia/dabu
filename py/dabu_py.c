#define PY_SSIZE_T_CLEAN
#include <stdbool.h>
#include <Python.h>

#include "../dabu.h"

static PyObject* dabu_dump(PyObject* self, PyObject* args) {
    const char* path = NULL;
    bool dump = false;

    if (!PyArg_ParseTuple(args, "si", &path, &dump)) {
        return PyList_New(0);
    }

    if (!path)
        return PyList_New(0);

    block_T *block = NULL;
    assembly_T *assembly_list = NULL;

    size_t count = assemblies_dump(&block, path, &assembly_list, dump);

    if (!assembly_list || count == 0)
        return PyList_New(0);

    PyObject *list = PyList_New(count);

    if (!list)
        return PyList_New(0);

    assembly_T *iter = assembly_list;

    size_t i = 0;

    while (iter  && i < count)
    {
        PyObject *dict = PyDict_New();
        if (!dict)
        {
            Py_DECREF(list);
            return PyList_New(0);
        }

        PyDict_SetItemString(dict, "name", PyUnicode_FromString(iter->name));
        PyDict_SetItemString(dict, "size", PyLong_FromLong(iter->size));
        PyList_SetItem(list, i, dict);
        iter = iter->next;
        i++;
    }

    block_free(&block);

    return list;
}

static PyMethodDef methods[] = {
    {"dump", dabu_dump, METH_VARARGS, "Unpacks DLLs from the assemblies.blob file and returns a list of DLLs, or an empty list on failure."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "dabu",
    ".NET assemblies blob unpacker module written in C",
    -1,
    methods
};

PyMODINIT_FUNC PyInit_dabu(void) {
    return PyModule_Create(&module);
}

