#ifndef R_MEM_H
#define R_MEM_H

// __attribute__((packed)) tells GCC not to add hidden spaces inside the struct.
// This guarantees that sizeof(BlockHeader) is exactly 12 bytes.
typedef struct BlockHeader {
    unsigned int size;
    int is_free;
    struct BlockHeader *next;
} __attribute__((packed)) BlockHeader;

void mem_init(void *heap_start, unsigned int heap_size);
void *r_malloc(unsigned int size);
void r_free(void *ptr);

#endif
