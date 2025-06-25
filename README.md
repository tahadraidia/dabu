# DABU - .NET Assembly Blob Unpacker
DABU is a tool for unpacking .NET assemblies (DLL files) from an `assemblies.blob` file. It is implemented in C and exposes an interface for easy integration and portability across other programming languages.

- A cli tool can be found under `cli` directory.
- Python bindings can be found under `py` directory.
- Java bindings are planned and currently in the pipe line.

## C Library Interface
DABU exposes a minimal C interface to enable easy integration into other projects or languages. Here's a summary of the core API:

```C
#ifndef _DABU_H
#define _DABU_H

#define MAX_NAME 1024

#include <stdint.h>

typedef struct assembly_T {
    char name[MAX_NAME];
    size_t size;
    struct assembly_T *next;
} assembly_T;

typedef struct block_T block_T;

size_t
assemblies_dump(block_T **, const char *, assembly_T **, const bool);

void
block_free(block_T **block);

#endif
```

`assemblies_dump()` accepts:  
- **IN** `block_T**`: memory arena pointer   
- **IN** `const char*`:  Path to blob file   
- **OUT** `assembly_T**`: linked list of assemblies found   
- **IN** `bool`: whether to extract DLLs to disk   

### Example (C)

```C
#include <stdio.h>
#include <stdlib.h>
#include "dabu.h"

...

int
main(int argc, char *argv[])
{
    const char *file = NULL;

    //TODO: parse flag to handle dlls extraction
    if (argc >= 2 && argv[1] && *argv[1])
        file = argv[1];
    else
        help(argv[0]);

    if (file && (strlen(file) > 1))
    {
            assembly_T *list = NULL;
            block_T *block = NULL;

            size_t count = assemblies_dump(&block, file, &list, false);

            if (list && count > 0)
            {
                    assembly_T *iter = list;
                    while (iter)
                    {
                            if (iter->name[0]) printf("%s\n", iter->name);
                            iter = iter->next;
                    }
            }

            block_free(&block);
    }

    return 0;
}
```

### CLI TOOL

##### Build

```sh
cd cli
mkdir build
cd build
cmake ../ -G Ninja
ninja
```

##### Usage

```sh
./dabu_cli assemblies.blob

```

Outputs a list of DLLs found in the blob. Add flags for extraction options. 

### Fuzzing

AFL++ was used to harden the parser against malformed .blob inputs.

##### Build for Fuzzing

```sh
mkdir fuzz && cd fuzz
cmake ../ -DFUZZ=ON
```

##### Run a Session

```
afl-fuzz -i in -o out -- ./fuzz_dabu_cli @@
```
- Ensure a valid `.blob` file is placed in `in/` directory.  
- The binary `fuzz_dabu_cli` is AFL-instrumented.  


##### Example Session: `7b99dd42d268d8b1826941096e5be55348641003`

- Observed: Segmentation faults, assertions failures  
- Fixes: Applied and verified in the follow up sessions   

###### Befores Fixes:

![](cli/fuzz/sc/discovery_7b99dd42d268d8b1826941096e5be55348641003.png)

###### After Fixes:

![](cli/fuzz/sc/bugfixes_7b99dd42d268d8b1826941096e5be55348641003.png)

## Python Bindings

DABU exposes a simple Python API built using CPython’s C-API.

### Exposed API

```C
...

static PyObject* dabu_dump(PyObject* self, PyObject* args) {
    const char *path = NULL;
    int dump = 0;
    
    if (!PyArg_ParseTuple(args, "si", &path, &dump)) {
        return PyList_New(0);
    }

...
```

##### Python Method Table

```C
static PyMethodDef methods[] = {
    {"dump", dabu_dump, METH_VARARGS, "Unpacks DLLs from the assemblies.blob file and returns a list of DLLs, or an empty list on failure."},
    {NULL, NULL, 0, NULL}
};
```

#### Build and Install

```sh
cd py
py .\setup.py build
py .\setup.py install
```

##### Example Usage

```py
import re
from sys import argv
from dabu import dump

# Filter common system assemblies
regex = r'^(System\.|Mono\.|.*_Microsoft|Microsoft|Xamarin|mscorlib|Newtonsoft|Java.Interop)'

if __name__ == "__main__":
	if (len(argv) - 1) < 1:
		print("{} <blob path>".format(argv[0]));
		exit(0);
	blob = argv[1]
	pattern = re.compile(regex)
	assemblies = dump(blob, False)
	if (len(assemblies) > 0):
		dlls = list(filter(lambda i: not pattern.match(i['name']), assemblies))
		print(dlls)

```

See `py/example.py` for usage of `dabu` module.

#### TODOs
- [ ] Add CLI flags for disk extraction  
- [ ] Fuzz Python C extension  
- [ ] Finalize Java binding via JNI  
