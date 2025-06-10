#ifndef UTILS_H
#define UTILS_H

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, new_capacity) \
    (type *)reallocate(pointer, sizeof(type) * (new_capacity))

#define FREE_ARRAY(pointer) reallocate(pointer, 0)

#define DECLARE_ARR(name, type) \
    typedef struct name { \
        int size; \
        int cap; \
        type *data; \
    } name;

#define ARR_INIT(stack) \
    do { \
        (stack)->cap = 0; \
        (stack)->size = 0; \
        (stack)->data = NULL; \
    } while(0)

#define ARR_PUSH(stack, element, type) \
    do { \
        if ((stack)->cap < (stack)->size + 1) { \
            size_t old_cap = (stack)->cap; \
            (stack)->cap = GROW_CAPACITY(old_cap); \
            (stack)->data = GROW_ARRAY(type, (stack)->data, (stack)->cap); \
        } \
        (stack)->data[(stack)->size] = (element); \
        (stack)->size++; \
    } while(0)

#define ARR_POP(stack) \
    do { \
        if ((stack)->size > 0) { \
            (stack)->size--; \
        } \
    } while(0)

#define ARR_TOP(stack) \
    ((stack)->size > 0 ? (stack)->data[(stack)->size - 1] : 0)

#define ARR_IS_EMPTY(stack) \
    ((stack)->size == 0)

#define ARR_FREE(stack) \
    do { \
        FREE_ARRAY((stack)->data); \
        ARR_INIT(stack); \
    } while(0)

void *reallocate(void *pointer, size_t new_size);

bool match(const char *str_lit, char *str_addr, size_t str_len);

#endif // UTILS_H
