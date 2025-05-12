#include <stdio.h>
#include <stdbool.h>

#include "../dabu.h"

int
main(int argc, char *argv[])
{
    const char *file = NULL;

	//TODO: parse flag to handle dlls extraction
    if (argc >= 2 && argv[1])
        file = argv[1];

    if (file && (strlen(file) > 1))
        assemblies_dump(file, true);

    return 0;
}
