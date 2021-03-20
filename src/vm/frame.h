#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "vm/page.h"

struct frame
{
    struct hash_elem elem;
    void *kaddr;
    struct page *page;
};

void frame_table_init(void);
void frame_table_free(void);

void *frame_alloc(struct page *);
void frame_free(void *kaddr, bool clear_pte);
#endif