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

static void *frame_evict(void)
{
    return NULL;
}

void *frame_alloc(struct page *page)
{
    ASSERT(page->kaddr == NULL);
    void *kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    if (kpage == NULL)
        kpage = frame_evict();
    ASSERT(kpage != NULL);
    page->kaddr = kpage;

    struct frame *f = malloc(sizeof(struct frame));
    f->kaddr = kpage;
    f->page = page;

    lock_acquire(&lock);
    hash_insert(&frame_table, &f->elem);
    lock_release(&lock);


    ASSERT(pagedir_get_page(thread_current()->pagedir, page->uaddr) == NULL);
    pagedir_set_page(thread_current()->pagedir, page->uaddr, kpage, page->writable);

    return kpage;
}

void frame_free(void *kaddr, bool clear_pte)
{
    struct frame *f = &(struct frame){.kaddr = kaddr};

    lock_acquire(&lock);
    struct hash_elem *e = hash_delete(&frame_table, &f->elem);
    ASSERT(e != NULL);
    lock_release(&lock);

    f = hash_entry(e, struct frame, elem);
    if (clear_pte)
        pagedir_clear_page(thread_current()->pagedir, f->page->uaddr);

    palloc_free_page(kaddr);
    free(f);
}