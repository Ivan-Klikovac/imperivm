#ifndef _IMPERIVM_TEMPLATES_VECTOR_H
#define _IMPERIVM_TEMPLATES_VECTOR_H

#define value_vector(T) \
\
typedef struct {\
    T* values;\
    uint64_t n_values;\
    uint64_t max_values;\
} vector_##T;\
\
static vector_##T* vector_##T##_new(void)\
{\
    vector_##T* v = malloc(sizeof(vector_##T));\
    v->max_values = 8;\
    v->n_values = 0;\
    v->values = calloc(8, sizeof(T));\
    return v;\
}\
\
static void vector_##T##_add(vector_##T* v, T value)\
{\
    if(v->n_values + 1 >= v->max_values) {\
        v->max_values *= 2;\
        v->values = realloc(v->values, v->max_values * sizeof(T));\
    }\
    v->values[v->n_values++] = value;\
}\
\
static void vector_##T##_remove(vector_##T* v, int index)\
{\
    for(int i = index; i < v->n_values - 1; i++) v->values[i] = v->values[i+1];\
    v->n_values--;\
}\
\
static void vector_##T##_clone(vector_##T* dst, vector_##T* src)\
{\
    if(dst->max_values <= src->n_values) {\
        dst->max_values = src->max_values;\
        dst->values = realloc(dst->values, src->max_values * sizeof(T));\
    }\
    memcpy(dst->values, src->values, src->n_values * sizeof(T));\
    dst->n_values = src->n_values;\
    dst->max_values = src->max_values;\
}\
\
static int vector_##T##_contains(vector_##T* v, T value)\
{\
    for(int i = 0; i < v->n_values; i++) if(v->values[i] == value) return 1;\
    return 0;\
}\
static void vector_##T##_free(vector_##T* v)\
{\
    free(v->values);\
    free(v);\
}

#define ptr_vector(T) \
\
typedef struct {\
    T** values;\
    uint64_t n_values;\
    uint64_t max_values;\
} vector_##T;\
\
static vector_##T* vector_##T##_new(void)\
{\
    vector_##T* v = malloc(sizeof(vector_##T));\
    v->max_values = 8;\
    v->n_values = 0;\
    v->values = calloc(8, sizeof(T*));\
    return v;\
}\
\
static void vector_##T##_add(vector_##T* v, T* value)\
{\
    if(v->n_values + 1 >= v->max_values) {\
        v->max_values *= 2;\
        v->values = realloc(v->values, v->max_values * sizeof(T*));\
    }\
    v->values[v->n_values++] = value;\
}\
\
static void vector_##T##_remove(vector_##T* v, int index)\
{\
    for(int i = index; i < v->n_values - 1; i++) v->values[i] = v->values[i+1];\
    v->n_values--;\
}\
\
static void vector_##T##_clone(vector_##T* dst, vector_##T* src)\
{\
    if(dst->max_values <= src->n_values) {\
        dst->max_values = src->max_values;\
        dst->values = realloc(dst->values, src->max_values * sizeof(T*));\
    }\
    memcpy(dst->values, src->values, src->n_values * sizeof(T*));\
    dst->n_values = src->n_values;\
    dst->max_values = src->max_values;\
}\
\
static int vector_##T##_contains(vector_##T* v, T* value)\
{\
    for(int i = 0; i < v->n_values; i++) if(v->values[i] == value) return 1;\
    return 0;\
}\
\
static int vector_##T##_find(vector_##T* v, T* value)\
{\
    for(int i = 0; i < v->n_values; i++) if(v->values[i] == value) return i;\
    return -1;\
}\
\
static void vector_##T##_ensure_capacity(vector_##T* v, int n)\
{\
    while(v->max_values < n) v->max_values *= 2;\
    v->values = realloc(v->values, n * sizeof(T*));\
}\
\
static void vector_##T##_free(vector_##T* v)\
{\
    free(v->values);\
    free(v);\
}

#endif