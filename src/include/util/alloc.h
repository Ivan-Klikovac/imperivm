#ifndef _IMPERIVM_ALLOC_H
#define _IMPERIVM_ALLOC_H

/*  A simple bump allocator for memory arenas
    
    Separate memory regions will be used for the AST, the IR, and the backend,
    and the arenas get freed when the compiler is done with them.
*/

#define ARENA_SIZE 1 << 30

typedef struct {
    void* base; // this is given by the OS
    void* ptr;  // and this walks forward with each allocation
} arena;

#endif