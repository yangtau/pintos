#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include <stddef.h>

typedef size_t swapid_t;
void swap_init(void);

// read from swap to main memory
// release a swap block
void swap_in(swapid_t, void *);

// write to swap
// allocate a swap block
swapid_t swap_out(void *);
#endif