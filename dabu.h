#ifndef _DABU_H
#define _DABU_H

#define MAX_NAME 1024

#include <stdint.h>
#include <stdbool.h>

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
