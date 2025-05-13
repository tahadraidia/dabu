from setuptools import setup, Extension

setup(
    name="dabu",
    version="1.0",
    ext_modules=[
        Extension("dabu", sources=["dabu_py.c", "../lz4.c", "../dabu.c" ]),
    ],
)

