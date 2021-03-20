#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>

#include "threads/thread.h"
#include "vm/swap.h"

enum page_type
{
    PAGE_ZERO,
    PAGE_SWAP,
    PAGE_FILESYS, // mmap
    PAGE_STACK,
    PAGE_HEAP,
};

struct page
{
    struct hash_elem elem;
    void *uaddr;
    void *kaddr;
    enum page_type type;
    bool writable;

    union
    {
        struct
        {
            int mapid; // if status is PAGE_FILESYS
            int off;   // a page may be part of a mmap
        } mmap;
        swapid_t swapid; // if status os PAGE_SWAP
    } as;
};

void page_table_init(struct thread *t);
void page_table_free(struct thread *t);

// page must not be present
bool page_add_mmap(void *upage, int mapid, int off, bool writable);
bool page_add_zero(void *upage, bool writable);
bool page_add_zeros(void *upage,size_t n, bool writable);

void page_clear(void *page);
bool page_load(void *upage);
#endif