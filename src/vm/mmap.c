#include "vm/mmap.h"

#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/file.h"

static unsigned mmap_entry_hash(const struct hash_elem *p_, void *AUX UNUSED)
{
    const struct mmap *p = hash_entry(p_, struct mmap, elem);
    return hash_bytes(&p->id, sizeof(int32_t));
}

static bool mmap_entry_less(const struct hash_elem *a_, const struct hash_elem *b_, void *AUX UNUSED)
{
    const struct mmap *a = hash_entry(a_, struct mmap, elem);
    const struct mmap *b = hash_entry(b_, struct mmap, elem);
    return a->id > b->id;
}

static void mmap_hash_free(struct hash_elem *e, void *aux UNUSED)
{
    struct mmap *m = hash_entry(e, struct mmap, elem);
    free(m);
}

void mmap_table_init(struct thread *t)
{
    t->mmap_next_id = 1;
    ASSERT(hash_init(&t->mmap_table, mmap_entry_hash, mmap_entry_less, NULL));
}

// free
void mmap_table_free(struct thread *t)
{
    hash_destroy(&t->mmap_table, mmap_hash_free);
}

static int next_mmapid(void)
{
    return thread_current()->mmap_next_id++;
}

static struct hash *current_mmap_table(void)
{
    return &thread_current()->mmap_table;
}

// If the size not a multiple of PGSIZE, then some bytes in the final mapped
// page "stick out" beyond the end of the file. Set these bytes to zero.
// return -1 on failure
int mmap_add(int fd, int off, size_t size, void *upage, bool writable)
{
    ASSERT((uint32_t)upage % PGSIZE == 0 && off >= 0 && size > 0);
    if (fd <= 1)
        return -1;

    int32_t mapid = next_mmapid();
    size_t map_off = 0;
    for (; map_off < size; map_off += PGSIZE)
    {
        if (!page_add_mmap(upage + map_off, mapid, map_off, writable))
        {
            size_t s = 0;
            for (; s < map_off; s += PGSIZE)
                page_clear(upage + s);
            return -1;
        }
    }

    // insert mmap into hash table
    struct mmap *entry = malloc(sizeof(struct mmap));
    ASSERT(entry != NULL);
    entry->id = mapid;
    entry->fd = fd;
    entry->offset = off;
    entry->size = size;
    entry->start_page = upage;
    ASSERT(hash_insert(current_mmap_table(), &entry->elem) == NULL);
    return mapid;
}

struct mmap *mmap_find(int mmapid)
{
    struct mmap entry = {.id = mmapid};
    struct hash_elem *e = hash_find(current_mmap_table(), &entry.elem);
    if (e == NULL)
        return NULL;
    return hash_entry(e, struct mmap, elem);
}

void mmap_remove(int mmapid)
{
    struct mmap *map = mmap_find(mmapid);
    ASSERT(map != NULL);
    ASSERT(hash_delete(current_mmap_table(), &map->elem) != NULL);

    size_t off = 0;
    for (; off < map->size; off += PGSIZE)
    {
        // TODO: write back
        page_clear(map->start_page + off);
    }
}

void mmap_load(int mmapid, void *kpage, int off)
{
    struct mmap *map = mmap_find(mmapid);
    ASSERT(map != NULL);

    struct file *f = process_fd_get(map->fd);
    ASSERT(f != NULL);
    int size = (int)map->size - off;
    ASSERT(size > 0);
    size = size > PGSIZE ? PGSIZE : size;
    ASSERT(file_read_at(f, kpage, size, map->offset + off) > 0);
}