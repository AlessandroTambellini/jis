#include "utils.h"

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

bool match(const char *str_lit, char *str_addr, size_t str_len)
{
    return strlen(str_lit) == str_len && strncmp(str_lit, str_addr, str_len) == 0;
}
