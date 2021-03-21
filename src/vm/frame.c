#include "vm/frame.h"

#include <hash.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"

static struct hash frame_table;
static struct lock lock;

static unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED)
{
    const struct frame *p = hash_entry(e, struct frame, elem);
    return hash_bytes(&p->kaddr, sizeof(void *));
}

static bool frame_less(const struct hash_elem *e, const struct hash_elem *f, void *aux UNUSED)
{
    const struct frame *p1 = hash_entry(e, struct frame, elem);
    const struct frame *p2 = hash_entry(f, struct frame, elem);
    return p1->kaddr < p2->kaddr;
}

void frame_table_init(void)
{
    ASSERT(hash_init(&frame_table, frame_hash, frame_less, NULL));
    lock_init(&lock);
}

void frame_table_free(void)
{
}

// CLOCK
static void *frame_evict(void)
{
    struct hash_iterator it;
    struct frame *f = NULL;

    lock_acquire(&lock);
    if (hash_empty(&frame_table))
        goto done;
    while (true)
    {
        // first phase: try to find an entry that (A=0, D=0)
        hash_first(&it, &frame_table);
        while (hash_next(&it))
        {
            f = hash_entry(hash_cur(&it), struct frame, elem);
            if (!page_access(f->page) && !page_dirty(f->page))
                goto done;
        }

        // second phase: try to find an entry that (A=0, D=1), and clear A
        hash_first(&it, &frame_table);
        while (hash_next(&it))
        {
            f = hash_entry(hash_cur(&it), struct frame, elem);
            if (!page_access(f->page))
                goto done;
            page_set_access(f->page, false);
        }
    }
    void *page = NULL;

done:
    if (f != NULL)
    {
        page = f->kaddr;
        ASSERT(page_unload(f->page->uaddr));
        // remove f in the hash table
        hash_delete(&frame_table, &f->elem);
        free(f);
    }
    lock_release(&lock);
    return page;
}

void *frame_alloc(const struct page *page)
{
    ASSERT(page->kaddr == NULL);
    void *kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    if (kpage == NULL)
        kpage = frame_evict();
    ASSERT(kpage != NULL);

    struct frame *f = malloc(sizeof(struct frame));
    f->kaddr = kpage;
    f->page = page;

    lock_acquire(&lock);
    hash_insert(&frame_table, &f->elem);
    lock_release(&lock);

    return kpage;
}

void frame_free(void *kaddr)
{
    struct frame *f = &(struct frame){.kaddr = kaddr};

    lock_acquire(&lock);
    struct hash_elem *e = hash_delete(&frame_table, &f->elem);
    ASSERT(e != NULL);
    lock_release(&lock);

    f = hash_entry(e, struct frame, elem);
    palloc_free_page(kaddr);
    free(f);
}