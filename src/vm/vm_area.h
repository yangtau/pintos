#ifndef VM_AREA_H
#define VM_AREA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "filesys/off_t.h"
#include "threads/thread.h"

typedef int32_t mapid_t;
// init
void vm_area_init(struct thread *t);

// free
void vm_area_free(struct thread *t);

// If the size not a multiple of PGSIZE, then some bytes in the final mapped
// page "stick out" beyond the end of the file. Set these bytes to zero.
// return -1 on failure
mapid_t vm_area_map(void *upage, int fd, off_t off, size_t size, bool writable);

// the size must be a multiple of PGSIZE
// return -1 on failure
bool vm_area_map_zero(void *upage, size_t size, bool writeable);

bool vm_area_load(void *upage);
#endif // vm/area.h