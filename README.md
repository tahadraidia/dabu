# DABU - .NET Assembly Blob Unpacker
DABU is a tool for unpacking .NET assemblies (DLL files) from an `assemblies.blob` file. It is implemented in C and exposes an interface for easy integration and portability across other programming languages.

- A cli tool can be found under `cli` directory.
- Python bindings can be found under `py` directory.
- Java bindings are planned and currently in the pipe line.

### CLI Build

```sh
cd cli
mkdir build
cd build
cmake ../ -G Ninja
ninja
```

### Python Build

```sh
cd py
py .\setup.py build
py .\setup.py install
```

See `py/example.py` for usage of `dabu` module.
