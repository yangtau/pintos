#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <stdbool.h>

// init frame table
void frame_table_init(void);

// map from frame (kaddr) to user page (pte)
void frame_table_insert(const void *kaddr, uint32_t *pte);

// remove mapping from frame (kaddr) to user page (pte)
void frame_table_remove(const void *kaddr, const uint32_t *pte);

#endif /* vm/frame.h */
