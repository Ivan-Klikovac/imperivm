#include <vector.h>
#include <string.h>
#include <stdlib.h>

vector* vector_new(void)
{
    vector* new = malloc(sizeof(vector));
    new->n_values = 0;
    new->max_values = 8;
    new->values = malloc(8 * sizeof(void*));

    return new;
}

void vector_add(vector* v, void* value)
{
    if(v->n_values + 1 >= v->max_values) {
        v->max_values *= 2;
        v->values = realloc(v->values, v->max_values * sizeof(void*));
    }
    v->values[v->n_values++] = value;
}

void vector_clone(vector* dst, vector* src)
{
    if(dst->max_values <= src->n_values) {
        dst->max_values = src->max_values; // to keep things consistent, all powers of 2
        dst->values = realloc(dst->values, src->max_values * sizeof(void*));
    }
    memcpy(dst->values, src->values, src->n_values * sizeof(void*));
    dst->n_values = src->n_values;
    dst->max_values = src->max_values;
}

int vector_contains(vector* v, void* value)
{
    if(!v) return 0;
    for(int i = 0; i < v->n_values; i++) if(v->values[i] == value) return 1;
    return 0;
}

int vector_find(vector* v, void* value)
{
    if(!v) return -1;
    for(int i = 0; i < v->n_values; i++) if(v->values[i] == value) return i;
    return -1;
}

void vector_remove(vector* v, int index)
{
    for(int i = index; i < v->n_values - 1; i++) v->values[i] = v->values[i+1];
    v->n_values--;
}
