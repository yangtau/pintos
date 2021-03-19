#include "vm/vm_area.h"

#include <hash.h>
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "filesys/file.h"

/* *
 * PTE
 * --------------------------------------------
 * |                           |   type   | 0 |
 * --------------------------------------------
 *           28 bits            3 bits    1 bit
 * */
#define PTE_VM_AREA_BITMAP (0xfffffff0)
#define PTE_VM_AREA_SIZE_CHECK(_x)       \
    do                                   \
    {                                    \
        uint32_t __x = (uint32_t)(_x);   \
        ASSERT((__x & 0xf0000000) == 0); \
    } while (0)

enum mem_area_type
{
    MEM_ZERO = 0x1,
    MEM_SWAP = 0x2,
    MEM_MAP = 0x3,
};

#define pte_vm_area_type(pte) ((enum mem_area_type)(((pte)&0xf) >> 1))
#define pte_vm_area_x(pte) ((uint32_t)(((pte)&PTE_VM_AREA_BITMAP) >> 4))

static void pte_set(uint32_t *pte, enum mem_area_type type, uint32_t x)
{
    uint32_t t = 0;
    t |= (x << 4);

    uint32_t tp = type & 0x07; // 3 bits
    t |= (tp << 1);

    *pte = t;
}

// mmap: id -> fd, offset, size
struct mmap_entry
{
    struct hash_elem elem;
    mapid_t id;
    int fd;
    off_t off;
    size_t size;
    bool writable;
    void *start_addr;
};

static unsigned mmap_entry_hash(const struct hash_elem *p_, void *AUX UNUSED)
{
    struct mmap_entry *p = hash_entry(p_, struct mmap_entry, elem);
    return hash_bytes(&p->id, sizeof(mapid_t));
}

static bool mmap_entry_less(const struct hash_elem *a_, const struct hash_elem *b_, void *AUX UNUSED)
{
    struct mmap_entry *a = hash_entry(a_, struct mmap_entry, elem);
    struct mmap_entry *b = hash_entry(b_, struct mmap_entry, elem);
    return a->id > b->id;
}

void vm_area_init(struct thread *t)
{
    t->next_mmap_id = 1;
    ASSERT(hash_init(&t->mem_map, mmap_entry_hash, mmap_entry_less, NULL));
}

static void mmap_hash_free(struct hash_elem *e, void *aux UNUSED)
{
    struct mmap_entry *m = hash_entry(e, struct mmap_entry, elem);
    free(m);
}

// free
void vm_area_free(struct thread *t)
{
    hash_destroy(&t->mem_map, mmap_hash_free);
}

// If the size not a multiple of PGSIZE, then some bytes in the final mapped
// page "stick out" beyond the end of the file. Set these bytes to zero.
// return -1 on failure
mapid_t vm_area_map(void *upage, int fd, off_t off, size_t size, bool writable)
{
    if ((uint32_t)upage % PGSIZE != 0)
        return -1;
    if (fd <= 1 || off < 0)
        return -1;

    struct thread *cur = thread_current();
    mapid_t mapid = cur->next_mmap_id++;
    PTE_VM_AREA_SIZE_CHECK(mapid);

    size_t offset = 0;
    for (; offset < size; offset += PGSIZE)
    {
        uint32_t *pte = pagedir_get_pte(cur->pagedir + offset, upage, true);
        ASSERT(pte != NULL);
        if (*pte != 0)
        {
            // clear all
            size_t s = 0;
            for (; s < offset; s += PGSIZE)
            {
                uint32_t *pte = pagedir_get_pte(cur->pagedir + offset, upage, true);
                ASSERT(pte != NULL);
                *pte = 0;
            }
            return -1;
        }
        pte_set(pte, MEM_MAP, mapid);
    }

    // insert mmap into hash table
    struct mmap_entry *entry = malloc(sizeof(struct mmap_entry));
    entry->id = mapid;
    entry->fd = fd;
    entry->off = off;
    entry->size = size;
    entry->writable = writable;
    entry->start_addr = upage;
    if (hash_insert(&cur->mem_map, &entry->elem) != NULL)
        goto fail;
    return mapid;
fail:
    free(entry);
    return -1;
}

// the size must be a multiple of PGSIZE
// return -1 on failure
bool vm_area_map_zero(void *upage, size_t size, bool writable)
{
    // TODO: writable
    if ((uint32_t)upage % PGSIZE != 0)
        return false;

    struct thread *cur = thread_current();

    size_t offset = 0;
    for (; offset < size; offset += PGSIZE)
    {
        uint32_t *pte = pagedir_get_pte(cur->pagedir + offset, upage, true);
        ASSERT(pte != NULL);
        if (*pte != 0)
        {
            // clear all
            size_t s = 0;
            for (; s < offset; s += PGSIZE)
            {
                uint32_t *pte = pagedir_get_pte(cur->pagedir + offset, upage, true);
                ASSERT(pte != NULL);
                *pte = 0;
            }
            return false;
        }
        pte_set(pte, MEM_ZERO, (int)writable);
    }
    return true;
}

// try to load
bool vm_area_load(void *upage)
{
    struct thread *cur = thread_current();
    uint32_t *pte = pagedir_get_pte(cur->pagedir, upage, false);
    if (pte == NULL)
        return false;

    enum mem_area_type type = pte_vm_area_type(*pte);
    uint32_t x = pte_vm_area_x(*pte);

    if (type == 0)
        return false;

    void *p = palloc_get_page(PAL_USER | PAL_ZERO);
    bool writable = false;
    if (type == MEM_SWAP)
    {
        // TODO:
    }
    else if (type == MEM_ZERO)
    {
        writable = (bool)x;
    }
    else if (type == MEM_MAP)
    {
        struct mmap_entry entry = {.id = (mapid_t)x};
        struct hash_elem *e = hash_find(&cur->mem_map, &entry.elem);
        struct mmap_entry *m = hash_entry(e, struct mmap_entry, elem);
        writable = m->writable;

        ASSERT((upage - m->start_addr) % PGSIZE == 0);
        off_t offset = m->off + (upage - m->start_addr);
        size_t size = m->size - (upage - m->start_addr);
        if (size > PGSIZE)
            size = PGSIZE;

        struct file *f = process_fd_get(m->fd);
        ASSERT(f != NULL);

        file_seek(f, offset);
        if (file_read(f, p, size) < 0)
            goto fail;
    }

    if (pagedir_set_page(cur->pagedir, upage, p, writable))
        return true;
    // TODO: need to refresh TLB?

fail:
    palloc_free_page(p);
    return false;
}