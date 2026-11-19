#include "memory.h"

#define HEAP_SIZE (64 * 1024)

#define MEMORY_ALIGNMENT 16

#define MIN_SPLIT_SIZE 16

static uint8_t heap[HEAP_SIZE]
    __attribute__((aligned(MEMORY_ALIGNMENT)));

static memory_chunk_t *heap_start = NULL;

static int memory_initialized = 0;

static int chunk_is_valid(const memory_chunk_t *chunk) {
    uintptr_t heap_begin = (uintptr_t)heap;
    uintptr_t heap_end = heap_begin + HEAP_SIZE;
    uintptr_t chunk_addr = (uintptr_t)chunk;

    if (chunk_addr < heap_begin || chunk_addr > heap_end) {
        return 0;
    }

    if (heap_end - chunk_addr < sizeof(memory_chunk_t)) {
        return 0;
    }

    if (chunk_addr % MEMORY_ALIGNMENT != 0) {
        return 0;
    }

    uintptr_t data_addr = chunk_addr + sizeof(memory_chunk_t);

    if (chunk->size > heap_end - data_addr) {
        return 0;
    }

    if (chunk->size % MEMORY_ALIGNMENT != 0) {
        return 0;
    }

    if (chunk->is_free != 0 && chunk->is_free != 1) {
        return 0;
    }

    if (chunk->next != NULL) {
        uintptr_t expected_next = data_addr + chunk->size;

        if ((uintptr_t)chunk->next != expected_next) {
            return 0;
        }
    }

    return 1;
}

static size_t align_size(size_t size) {
    return (size + MEMORY_ALIGNMENT - 1)
           & ~(MEMORY_ALIGNMENT - 1);
}

static void merge_free_chunks(void) {
    memory_chunk_t *current = heap_start;

    while (current != NULL) {

        if (!chunk_is_valid(current)) {
            return;
        }

        memory_chunk_t *next = current->next;

        if (next == NULL) {
            break;
        }

        if (!chunk_is_valid(next)) {
            return;
        }

        if (current->is_free && next->is_free) {

            current->size += sizeof(memory_chunk_t)
                             + next->size;

            current->next = next->next;

        } else {
            current = next;
        }
    }
}

void memory_init(void) {
    if (memory_initialized) {
        return;
    }

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

    if (size > HEAP_SIZE - sizeof(memory_chunk_t)) {
        return NULL;
    }

    memory_chunk_t *current = heap_start;

    while (current != NULL) {

    if (!chunk_is_valid(current)) {
        return NULL;
    }

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

        if (!chunk_is_valid(current)) {
            return;
        }

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
        if (!chunk_is_valid(current)) {
            return 0;
        }

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
        if (!chunk_is_valid(current)) {
            return 0;
        }

        if (current->is_free) {
            free_memory += current->size;
        }

        current = current->next;
    }

    return free_memory;
}