#ifndef _IMPERIVM_TEMPLATES_GRAPH_H
#define _IMPERIVM_TEMPLATES_GRAPH_H

#include <stdint.h>
#include <templates/vector.h>

#define graph(T) \
\
typedef struct {\
    vector_##T* nodes;\
    int** matrix;\
    int* colors;\
} graph_##T;\
\
static graph_##T* graph_##T##_new(vector_##T* v)\
{\
    graph_##T* g = malloc(sizeof(graph_##T));\
    g->nodes = v;\
    g->matrix = malloc(v->n_values * sizeof(int*));\
    for(int i = 0; i < v->n_values; i++)\
        g->matrix[i] = calloc(v->n_values, sizeof(int));\
    g->colors = 0;\
    return g;\
}

#endif