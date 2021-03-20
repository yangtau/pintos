#include "vm/page.h"

#include <debug.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/mmap.h"

static unsigned page_hash(const struct hash_elem *e, void *aux UNUSED)
{
    const struct page *p = hash_entry(e, struct page, elem);
    return hash_bytes(&p->uaddr, sizeof(void *));
}

static bool page_less(const struct hash_elem *e, const struct hash_elem *f, void *aux UNUSED)
{
    const struct page *a = hash_entry(e, struct page, elem);
    const struct page *b = hash_entry(f, struct page, elem);
    return a->uaddr < b->uaddr;
}

static void page_free(struct hash_elem *e, void *aux UNUSED)
{
    struct page *p = hash_entry(e, struct page, elem);
    if (p->kaddr != NULL) {
        frame_free(p->kaddr, true);
    }
    free(p);
}

void page_table_init(struct thread *t)
{
    ASSERT(hash_init(&t->page_table, page_hash, page_less, NULL));
}

void page_table_free(struct thread *t)
{
    hash_destroy(&t->page_table, page_free);
}

static struct hash *current_page_table(void)
{
    return &thread_current()->page_table;
}

// page must not be present
bool page_add_mmap(void *upage, int mapid, int off, bool writable)
{
    struct page *p = &(struct page){.uaddr = upage};
    if (hash_find(current_page_table(), &p->elem) != NULL)
        return false;

    p = calloc(1, sizeof(struct page));
    ASSERT(p != NULL);

    p->uaddr = upage;
    p->type = PAGE_FILESYS;
    p->writable = writable;
    p->as.mmap.mapid = mapid;
    p->as.mmap.off = off;

    ASSERT(hash_insert(current_page_table(), &p->elem) == NULL);
    return true;
}

bool page_add_zero(void *upage, bool writable)
{
    struct page *p = &(struct page){.uaddr = upage};
    if (hash_find(current_page_table(), &p->elem) != NULL)
        return false;

    p = calloc(1, sizeof(struct page));
    ASSERT(p != NULL);

    p->uaddr = upage;
    p->type = PAGE_ZERO;
    p->writable = writable;

    ASSERT(hash_insert(current_page_table(), &p->elem) == NULL);
    return true;
}

//  add n zero page start from upage
bool page_add_zeros(void *upage, size_t n, bool writable)
{
    size_t i, j;
    for (i = 0; i < n; i++)
        if (!page_add_zero(upage + PGSIZE * i, writable))
        {
            for (j = 0; j < i; j++)
                page_clear(upage + PGSIZE * i);
            return false;
        }
    return true;
}

void page_clear(void *upage)
{
    struct page *p = &(struct page){.uaddr = upage};
    struct hash_elem *e = hash_find(current_page_table(), &p->elem);
    ASSERT(e != NULL);

    page_free(e, NULL);
    
    // p = hash_entry(e, struct page, elem);

    // // TODO: free it if it use a frame?
    // if (p->kaddr != NULL)
    //     frame_free(p->kaddr, true);

    hash_delete(current_page_table(), e);
    free(p);
}

bool page_load(void *upage)
{
    struct page *p = &(struct page){.uaddr = upage};
    struct hash_elem *e = hash_find(current_page_table(), &p->elem);
    ASSERT(e != NULL);
    p = hash_entry(e, struct page, elem);

    ASSERT(p->kaddr == NULL);
    p->kaddr = frame_alloc(p);
    ASSERT(p->kaddr != NULL);

    switch (p->type)
    {
    case PAGE_ZERO:
        memset(p->kaddr, 0, PGSIZE);
        return true;
    case PAGE_SWAP:
        swap_in(p->as.swapid, p->kaddr);
        return true;
    case PAGE_FILESYS:
        mmap_load(p->as.mmap.mapid, p->kaddr, p->as.mmap.off);
        return true;
    case PAGE_STACK:
        return true;
    case PAGE_HEAP:
        return true;
    default:
        break;
    }
    return false;
}