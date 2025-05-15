#include <stdio.h>
#include <stdlib.h>
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
    {
		assembly_T *list = NULL;
		block_T *block = NULL;
		size_t count = assemblies_dump(&block, file, &list, false);
        if (list && count > 0)
        {
            assembly_T *iter = list;
            while (iter)
            {
                if (iter->name) printf("%s\n", iter->name);
                iter = iter->next;
            }
        }
		block_free(&block);
    }

    return 0;
}
