#ifndef _IMPERIVM_TEMPLATES_HASHMAP_H
#define _IMPERIVM_TEMPLATES_HASHMAP_H

#define SIZE 1009 // sufficiently large prime number, leave it like this for now

#define HASH(x) (((((uint64_t)x)+1) << 1) % SIZE)

#define hashmap(T_key, T_value) \
\
typedef struct { \
    T_key* keys[SIZE]; \
    T_value* values[SIZE]; \
} hashmap_##T_key##_##T_value; \
\
static hashmap_##T_key##_##T_value* hashmap_##T_key##_##T_value##_new(void) \
{ \
    hashmap_##T_key##_##T_value* h = calloc(1, sizeof(hashmap_##T_key##_##T_value)); \
    return h; \
} \
\
static void hashmap_##T_key##_##T_value##_add(hashmap_##T_key##_##T_value* h, T_key* key, T_value* value) \
{ \
    int hash = HASH(key); \
    while(h->keys[hash]) hash = HASH(key); \
    h->keys[hash] = key; \
    h->values[hash] = value; \
} \
\
static T_value* hashmap_##T_key##_##T_value##_get(hashmap_##T_key##_##T_value* h, T_key* key) \
{ \
    int hash = HASH(key); \
    while(h->keys[hash] != key) hash = HASH(key); \
    return h->values[hash]; \
} \
\
static int hashmap_##T_key##_##T_value##_has_key(hashmap_##T_key##_##T_value* h, T_key* key) \
{ \
    int hash = HASH(key); \
    while(h->keys[hash] && h->keys[hash] != key) hash = HASH(key); \
    if(h->keys[hash]) return 1; \
    return 0; \
}

#endif