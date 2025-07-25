#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "../dabu.h"

static PyObject* dabu_dump(PyObject* self, PyObject* args) {
    const char *path = NULL;
    int dump = 0;

    if (!PyArg_ParseTuple(args, "si", &path, &dump)) {
        return PyList_New(0);
    }

    if (!path || !(*path) || strlen(path) <= 0)
        return PyList_New(0);

    block_T *block = NULL;
    assembly_T *assembly_list = NULL;

    size_t count = assemblies_dump(&block, path, &assembly_list, dump);

    if (!assembly_list || count == 0)
        return PyList_New(0);

    PyObject *list = PyList_New(0);

    if (!list)
        return PyList_New(0);

    assembly_T *iter = assembly_list;

    while (iter)
    {
        PyObject *dict = PyDict_New();
        if (!dict)
        {
            Py_XDECREF(list);
            return PyList_New(0);
        }

        if (iter->name[0] != '\0' || !iter->size)
        {
            iter = iter->next;
        }

        PyObject *name = PyUnicode_FromString(iter->name);
        PyObject *size = PyLong_FromLong(iter->size);
        if (!name || !size)
        {
            Py_XDECREF(name);
            Py_XDECREF(size);
            Py_DECREF(dict);
            return PyList_New(0);
        }

        PyDict_SetItemString(dict, "name", name);
        PyDict_SetItemString(dict, "size", size);

        Py_DECREF(name);
        Py_DECREF(size);

        if (PyList_Append(list, dict) < 0)
        {
            Py_DECREF(dict);
            Py_DECREF(list);
            return PyList_New(0);
        }

        Py_DECREF(dict);

        iter = iter->next;
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

