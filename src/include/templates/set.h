#ifndef _IMPERIVM_TEMPLATES_SET_H
#define _IMPERIVM_TEMPLATES_SET_H

#include <stdarg.h>

// presupposes that T is a struct that has an enum member `type`
// you pass it a ptr to the struct, the number of enum args, and the enum args themselves
// returns 1 if the type enum is any of those values, 0 otherwise
#define type_set(T) \
static int T##_is(T* p, int n, ...)\
{\
    va_list list;\
    va_start(list, n);\
    for(int i = 0; i < n; i++) if(p->type == va_arg(list, int)) {\
        va_end(list);\
        return 1;\
    }\
    va_end(list);\
    return 0;\
}

// requires a vector of type T to be defined already
// replaces vector_T_add() such that it doesn't add duplicates
#define vector_set(T) \
\
static void vector_set_##T##_add(vector_##T* v, T* value) \
{ \
    if(vector_##T##_contains(v, value)) return; \
    \
    if(v->n_values + 1 >= v->max_values) {\
        v->max_values *= 2;\
        v->values = realloc(v->values, v->max_values * sizeof(T*));\
    }\
    v->values[v->n_values++] = value;\
}

#endif