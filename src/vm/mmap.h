#ifndef VM_AREA_H
#define VM_AREA_H

#include <stdbool.h>
#include <hash.h>

#include "threads/thread.h"
#include "vm/page.h"
#include "filesys/file.h"

struct mmap
{
    int id;
    struct file *file;
    size_t size;
    int offset;
    bool writeback;
    void *start_page;
    struct hash_elem elem;
};

void mmap_table_init(struct thread *t);
void mmap_table_free(struct thread *t);

int mmap_add(struct file *f, int offset, size_t size, void *p, bool writable, bool writeback);
struct mmap *mmap_find(int mmapid);
void mmap_remove(int mmapid);
void mmap_load(int mmapid, void *page, int off);
// void mmap_unload(int mmapid, void *page, int off);
#endif // vm/area.h