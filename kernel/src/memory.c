#include "memory.h"

#define HEAP_SIZE (64 * 1024)

#define MEMORY_ALIGNMENT 16

#define MIN_SPLIT_SIZE 16

static uint8_t heap[HEAP_SIZE]
    __attribute__((aligned(MEMORY_ALIGNMENT)));

static memory_chunk_t *heap_start = NULL;

static int memory_initialized = 0;

static size_t align_size(size_t size) {
    return (size + MEMORY_ALIGNMENT - 1)
           & ~(MEMORY_ALIGNMENT - 1);
}

static void merge_free_chunks(void) {
    memory_chunk_t *current = heap_start;

    while (current != NULL && current->next != NULL) {
        memory_chunk_t *next = current->next;

        if (current->is_free && next->is_free) {
            current->size += sizeof(memory_chunk_t)
                             + next->size;

            current->next = next->next;
        } else {
            current = current->next;
        }
    }
}

void memory_init(void) {
    heap_start = (memory_chunk_t *)heap;

    heap_start->size =
        HEAP_SIZE - sizeof(memory_chunk_t);
    
    heap_start->is_free = 1;
    heap_start->next = NULL;
    memory_initialized = 1;
}

void *kmalloc(size_t size) {
    if (!memory_initialized || size == 0) {
        return NULL;
    }

    if (size > (size_t)-1 - (MEMORY_ALIGNMENT - 1)) {
        return NULL;
    }

    size = align_size(size);

    memory_chunk_t *current = heap_start;

    while (current != NULL) {
        if (current->is_free && current->size >= size) {

            size_t remaining = current->size - size;

            if (remaining >= sizeof(memory_chunk_t)
                             + MIN_SPLIT_SIZE) {

                memory_chunk_t *new_chunk =
                    (memory_chunk_t *)(
                        (uint8_t *)(current + 1) + size
                    );

                new_chunk->size =
                    remaining - sizeof(memory_chunk_t);

                new_chunk->is_free = 1;
                new_chunk->next = current->next;

                current->next = new_chunk;
                current->size = size;
            }
            
            current->is_free = 0;
            return (void *)(current + 1);
        }

        current = current->next;
    }

    return NULL;
}

void kfree(void *ptr) {
    if (!memory_initialized || ptr == NULL) {
        return;
    }

    memory_chunk_t *current = heap_start;

    while (current != NULL) {

        if ((void *)(current + 1) == ptr) {
            if (!current->is_free) {
                current->is_free = 1;
                merge_free_chunks();
            }
            return;
        }
        current = current->next;
    }
}

size_t memory_get_total(void) {
    if (!memory_initialized) {
        return 0;
    }
    return HEAP_SIZE - sizeof(memory_chunk_t);
}

size_t memory_get_used(void) {
    if (!memory_initialized) {
        return 0;
    }

    size_t used = 0;
    memory_chunk_t *current = heap_start;

    while (current != NULL) {
        if (!current->is_free) {
            used += current->size;
        }
        current = current->next;
    }
    return used;
}

size_t memory_get_free(void) {
    if (!memory_initialized) {
        return 0;
    }
    size_t free_memory = 0;
    memory_chunk_t *current = heap_start;

    while (current != NULL) {
        if (current->is_free) {
            free_memory += current->size;
        }
        current = current->next;
    }
    return free_memory;
}