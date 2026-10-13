#include "mem.h"

static BlockHeader *first_block = 0;

void mem_init(void *heap_start, unsigned int heap_size) {
    first_block = (BlockHeader *)heap_start;
    first_block->size = heap_size - sizeof(BlockHeader);
    first_block->is_free = 1;
    first_block->next = 0;
}

void *r_malloc(unsigned int size) {
    BlockHeader *current = first_block;
    while (current != 0) {
        if (current->is_free == 1 && current->size >= size) {
            if (current->size > size + sizeof(BlockHeader)) {
                BlockHeader *new_block = (BlockHeader *)((unsigned char *)current + sizeof(BlockHeader) + size);
                new_block->size = current->size - (size + sizeof(BlockHeader));
                new_block->is_free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->is_free = 0;
            return (void *)((unsigned char *)current + sizeof(BlockHeader));
        }

        current = current->next;
    }

    return 0;
}

void r_free(void *ptr) {
    if (!ptr) return;
    BlockHeader *header = (BlockHeader *)((unsigned char *)ptr - sizeof(BlockHeader));
    header->is_free = 1;

    BlockHeader *current = first_block;

    while (current != 0 && current->next != 0) {
        if (current->is_free == 1 && current->next->is_free == 1) {
            current->size += sizeof(BlockHeader) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}