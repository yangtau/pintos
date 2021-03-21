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
    PAGE_NONE,
};

struct page
{
    struct hash_elem elem;
    const void *uaddr;
    void *kaddr;
    uint32_t *pte;
    enum page_type type;
    bool writable;

    struct
    {
        int mapid; // if status is PAGE_FILESYS
        int off;   // a page may be part of a mmap
    } mmap;
    swapid_t swapid; // if status os PAGE_SWAP
};

void page_table_init(struct thread *t);
void page_table_free(struct thread *t);

// page must not be present
bool page_add_mmap(const void *upage, int mapid, int off, bool writable);
bool page_add_zero(const void *upage, bool writable);
bool page_add_zeros(const void *upage, size_t n, bool writable);
bool page_add_stack(const void *upage, size_t n, bool writable);

void page_clear(const void *page);
bool page_load(const void *upage);
bool page_unload(const void *upage);
bool page_exists(const void *upage);

bool page_dirty(const struct page *p);
bool page_access(const struct page *p);
void page_set_access(const struct page *p, bool a);
void page_pte_clear(const struct page *p);
#endif