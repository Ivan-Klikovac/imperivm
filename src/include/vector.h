#ifndef _IMPERIVM_VECTOR_H
#define _IMPERIVM_VECTOR_H

#include <stdint.h>

typedef struct {
    uint64_t n_values;
    uint64_t max_values;
    void** values;
} vector;

vector* vector_new(void);
void vector_add(vector*, void*);
void vector_clone(vector*, vector*);
int vector_contains(vector*, void*);
int vector_find(vector*, void*);
void vector_remove(vector*, int);

#endif