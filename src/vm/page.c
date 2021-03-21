#include "vm/page.h"

#include <debug.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
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
    if (p->kaddr != NULL)
        frame_free(p->kaddr);
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

static struct page *new_page(const void *upage, enum page_type type, bool writable)
{
    struct page *p = calloc(1, sizeof(struct page));
    ASSERT(p != NULL);
    p->uaddr = upage;
    p->type = type;
    p->writable = writable;
    p->pte = pagedir_get_pte(thread_current()->pagedir, upage, true);
    return p;
}

// page must not be present
bool page_add_mmap(const void *upage, int mapid, int off, bool writable)
{
    struct page *p = &(struct page){.uaddr = upage};
    if (hash_find(current_page_table(), &p->elem) != NULL)
        return false;

    p = new_page(upage, PAGE_FILESYS, writable);
    p->mmap.mapid = mapid;
    p->mmap.off = off;

    ASSERT(hash_insert(current_page_table(), &p->elem) == NULL);
    return true;
}

bool page_add_zero(const void *upage, bool writable)
{
    struct page *p = &(struct page){.uaddr = upage};
    if (hash_find(current_page_table(), &p->elem) != NULL)
        return false;

    p = new_page(upage, PAGE_ZERO, writable);

    ASSERT(hash_insert(current_page_table(), &p->elem) == NULL);
    return true;
}

//  add n zero page start from upage
bool page_add_zeros(const void *upage, size_t n, bool writable)
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

bool page_add_stack(const void *upage, size_t n, bool writable)
{
    size_t i, j;
    for (i = 0; i < n; i++)
    {
        const void *uaddr = upage + i * PGSIZE;
        struct page *p = &(struct page){.uaddr = uaddr};
        if (hash_find(current_page_table(), &p->elem) != NULL)
        {
            for (j = 0; j < i; j++)
                page_clear(upage + PGSIZE * i);
            return false;
        }

        p = new_page(uaddr, PAGE_STACK, writable);
        ASSERT(hash_insert(current_page_table(), &p->elem) == NULL);
    }
    return true;
}

void page_clear(const void *upage)
{
    struct page *p = &(struct page){.uaddr = upage};
    struct hash_elem *e = hash_find(current_page_table(), &p->elem);
    ASSERT(e != NULL);

    p = hash_entry(e, struct page, elem);
    if (p->type == PAGE_SWAP)
        swap_release(p->swapid);

    if (p->kaddr != NULL)
        frame_free(p->kaddr);

    hash_delete(current_page_table(), e);
    free(p);
}

bool page_load(const void *upage)
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
        break;
    case PAGE_FILESYS:
        mmap_load(p->mmap.mapid, p->kaddr, p->mmap.off);
        break;
    case PAGE_SWAP:
        swap_in(p->swapid, p->kaddr);
        break;
    case PAGE_STACK:
        break;
    default:
        ASSERT(false);
        break;
    }

    *p->pte = pte_create_user(p->kaddr, p->writable);
    p->type = PAGE_NONE;
    return true;
}

// may be called by other thread
// TODO: lock
bool page_unload(const void *upage)
{
    struct page *p = &(struct page){.uaddr = upage};
    struct hash_elem *e = hash_find(current_page_table(), &p->elem);
    ASSERT(e != NULL);
    p = hash_entry(e, struct page, elem);

    ASSERT(p->kaddr != NULL);
    void *kpage = p->kaddr;
    p->kaddr = NULL;

    page_pte_clear(p);

    ASSERT(p->type == PAGE_NONE);

    p->swapid = swap_out(kpage);
    p->type = PAGE_SWAP;

    return true;
}

bool page_exists(const void *upage)
{
    ASSERT((uint32_t)upage % PGSIZE == 0);
    struct page *p = &(struct page){.uaddr = upage};
    struct hash_elem *e = hash_find(current_page_table(), &p->elem);
    return e != NULL;
}

bool page_dirty(const struct page *p)
{
    return !!(*p->pte & PTE_D);
}

bool page_access(const struct page *p)
{
    return !!(*p->pte & PTE_A);
}

void page_set_access(const struct page *p, bool a)
{
    if (a)
        *p->pte |= PTE_A;
    else
        *p->pte &= ~(uint32_t)(PTE_A);
}

void page_pte_clear(const struct page *p)
{
    *p->pte &= ~(uint32_t)PTE_P;
}