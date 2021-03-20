#ifndef VM_AREA_H
#define VM_AREA_H

#include <stdbool.h>
#include <hash.h>

#include "threads/thread.h"
#include "vm/page.h"

struct mmap
{
    int id;
    int fd;
    size_t size;
    int offset;
    void *start_page;
    struct hash_elem elem;
};

void mmap_table_init(struct thread *t);
void mmap_table_free(struct thread *t);

int mmap_add(int fd, int offset, size_t size, void *p, bool writable);
struct mmap *mmap_find(int mmapid);
void mmap_remove(int mmapid);
void mmap_load(int mmapid, void *kpage, int off);

#endif // vm/area.h