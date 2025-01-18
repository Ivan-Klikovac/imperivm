#ifndef _IMPERIVM_TEMPLATES_NAMED_VECTOR_H
#define _IMPERIVM_TEMPLATES_NAMED_VECTOR_H

// Assumes a pointer vector for T is already defined
// Adds, checks, and removes values from the vector not based on the pointer but on a string
// T must be a type that has a member `name`

#define named_vector(T) \
\
static int named_vector_##T##_contains(vector_##T* v, T* value) \
{\
    for(int i = 0; i < v->n_values; i++) \
        if(strcmp(v->values[i]->name, value->name) == 0) \
            return 1; \
    return 0; \
}\
\
static T* named_vector_##T##_find(vector_##T* v, T* value) \
{\
    for(int i = 0; i < v->n_values; i++) \
        if(strcmp(v->values[i]->name, value->name) == 0) \
            return v->values[i]; \
    return 0; \
}

#endif