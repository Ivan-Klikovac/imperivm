#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <util/alloc.h>

arena* arena_new(void)
{
    void* base = mmap(NULL, ARENA_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if(base == MAP_FAILED) {
        puts(strerror(errno));
        exit(1);
    }

    // bootstrap the arena
    arena* a = base;
    a->base = base;
    a->ptr = base + sizeof(arena);

    return a;
}

// Allocates size bytes in the given arena. The caller ensures its alignment.
void* arena_alloc(arena* a, size_t size) 
{
    a->ptr += size;
    return a->ptr - size;
}

void arena_free(arena* a)
{
    int res = munmap(a->base, ARENA_SIZE);
    
    if(res) {
        puts(strerror(errno));
        exit(0);
    }
}