#include <stdlib.h>

#include "array.h"

void *reallocate(void *pointer, size_t new_size) 
{
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void *res = realloc(pointer, new_size);
    
    if (res == NULL) exit(1); // TODO improve this exit to make more clear what happened

    return res;
}

