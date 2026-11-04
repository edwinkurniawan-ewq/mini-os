#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

typedef struct memory_chunk {
    size_t size;                
    int is_free;                
    struct memory_chunk *next;  
} memory_chunk_t;

void memory_init(void); 

void *kmalloc(size_t size); 

void kfree(void *ptr); 

size_t memory_get_total(void);
size_t memory_get_used(void);
size_t memory_get_free(void);

#endif
