#include "vm/frame.h"

#include <list.h>
#include <hash.h>
#include <debug.h>

#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "vm/vm_area.h"
#include "vm/swap.h"

// kaddr -> list of frame_entry
struct frame_entry
{
    struct hash_elem elem;
    void *addr; // the kernel address of the frame
    struct list list;
};

// list of of PTEs of user pages that refers to the frame
struct pte_elem
{
    uint32_t *pte;
    struct list_elem elem;
};

static struct hash frame_table;
static struct lock table_lock;

/* Returns a hash value for frame p. */
static unsigned frame_entry_hash(const struct hash_elem *p_, void *aux UNUSED)
{
    struct frame_entry *p = hash_entry(p_, struct frame_entry, elem);
    return hash_bytes(&p->addr, sizeof p->addr);
}

/* Returns true if frame a frame page b. */
static bool frame_entry_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
    const struct frame_entry *a = hash_entry(a_, struct frame_entry, elem);
    const struct frame_entry *b = hash_entry(b_, struct frame_entry, elem);

    return a->addr < b->addr;
}

void frame_table_init()
{
    ASSERT(hash_init(&frame_table, frame_entry_hash, frame_entry_less, NULL));
    lock_init(&table_lock);
}

void frame_table_insert(void *kaddr, uint32_t *pte)
{
    struct pte_elem *pte_elem = malloc(sizeof(struct pte_elem));
    pte_elem->pte = pte;

    struct frame_entry entry = {
        .addr = kaddr,
    };

    lock_acquire(&table_lock);
    struct hash_elem *elem = hash_find(&frame_table, &entry.elem);
    if (elem == NULL)
    {
        struct frame_entry *e = malloc(sizeof(struct frame_entry));
        ASSERT(e != NULL);
        e->addr = kaddr;
        list_init(&e->list);
        elem = &e->elem;

        ASSERT(hash_insert(&frame_table, &e->elem) == NULL);
    }

    list_push_back(&hash_entry(elem, struct frame_entry, elem)->list, &pte_elem->elem);
    lock_release(&table_lock);
}

// remove mapping from frame (kaddr) to user page (pte)
void frame_table_remove(void *kaddr, uint32_t *pte)
{
    struct frame_entry entry = {
        .addr = kaddr,
    };

    lock_acquire(&table_lock);
    struct hash_elem *elem = hash_find(&frame_table, &entry.elem);
    ASSERT(elem != NULL);

    // PTE list
    struct list *list = &hash_entry(elem, struct frame_entry, elem)->list;
    struct list_elem *e;
    for (e = list_begin(list); e != list_end(list); e = list_next(e))
    {
        if (list_entry(e, struct pte_elem, elem)->pte == pte)
        {
            list_remove(e);
            if (list_empty(list))
            {
                // free page
                palloc_free_page(kaddr);
                ASSERT(hash_delete(&frame_table, elem) != NULL);
            }
            lock_release(&table_lock);
            return;
        }
    }

    // there must be an elem be removed in the loop
    NOT_REACHED();
}

static void swap_out_frame(struct frame_entry *f)
{
    struct list_elem *e;

    bool dirty = false;
    for (e = list_begin(&f->list); e != list_end(&f->list); e = list_next(e))
    {
        struct pte_elem *p = list_entry(e, struct pte_elem, elem);
        dirty = dirty || pte_get_dirty(*p->pte);

        if (dirty)
        {
        }

        // TODO: no sharing
        break;
    }
}

// CLOCK
void *frame_table_evict()
{
    struct hash_iterator it;
    struct list_elem *e;
    struct frame_entry *f = NULL;

    lock_acquire(&table_lock);
    if (hash_empty(&frame_table))
        goto done;
    while (true)
    {
        // first phase: try to find an entry that (A=0, D=0)
        hash_first(&it, &frame_table);
        while (hash_next(&it))
        {
            bool access = false;
            bool dirty = false;
            f = hash_entry(hash_cur(&it), struct frame_entry, elem);
            for (e = list_begin(&f->list); e != list_end(&f->list); e = list_next(e))
            {
                struct pte_elem *p = list_entry(e, struct pte_elem, elem);
                access = access || pte_get_access(*p->pte);
                dirty = dirty || pte_get_dirty(*p->pte);
            }
            if (!access && !dirty)
                goto done;
        }

        // second phase: try to find an entry that (A=0, D=1), and clear A
        hash_first(&it, &frame_table);
        while (hash_next(&it))
        {
            bool access = false;
            f = hash_entry(hash_cur(&it), struct frame_entry, elem);
            for (e = list_begin(&f->list); e != list_end(&f->list); e = list_next(e))
            {
                struct pte_elem *p = list_entry(e, struct pte_elem, elem);
                access = access || pte_get_access(*p->pte);
                // clear access
                pte_clear_access(p->pte);
            }
            if (!access)
                goto done;
        }
    }
    void *page = NULL;
done:
    if (f != NULL)
    {
        page = f->addr;
        // remove f in the hash table
        hash_delete(&frame_table, &f->elem);
        swap_out_frame(f);
    }
    lock_release(&table_lock);
    return page;
}