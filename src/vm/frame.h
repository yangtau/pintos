#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <stdbool.h>

// init frame table
void frame_table_init(void);

// map from frame (kaddr) to user page (pte)
void frame_table_insert(void *kaddr, uint32_t *pte);

// remove mapping from frame (kaddr) to user page (pte)
void frame_table_remove(void *kaddr, uint32_t *pte);

// return the kernel address of the frame
// return null on fail
void *frame_table_evict(void);

#endif /* vm/frame.h */
