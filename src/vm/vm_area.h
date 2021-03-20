#ifndef VM_AREA_H
#define VM_AREA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "filesys/off_t.h"
#include "threads/thread.h"
#include "vm/swap.h"

// init
void vm_area_init(struct thread *t);

// free
void vm_area_free(struct thread *t);

// If the size not a multiple of PGSIZE, then some bytes in the final mapped
// page "stick out" beyond the end of the file. Set these bytes to zero.
// return -1 on failure
int32_t vm_area_map(void *upage, int fd, off_t off, size_t size, bool writable);

void vm_area_unmap(int32_t mapid);

// the size must be a multiple of PGSIZE
// return -1 on failure
void vm_area_zero(void *upage, size_t size, bool writable);

void vm_area_swap(uint32_t *pte, swapid_t id);

bool vm_area_load(void *upage);
#endif // vm/area.h