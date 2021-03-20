#include "vm/swap.h"

#include <bitmap.h>

#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

static struct block *swap_block;
static struct bitmap *swap_bits;
static size_t block_cnt;
static struct lock lock;

void swap_init()
{
    swap_block = block_get_role(BLOCK_SWAP);
    ASSERT(swap_block != NULL);

    block_sector_t size = block_size(swap_block);
    block_cnt = size * BLOCK_SECTOR_SIZE / PGSIZE;

    swap_bits = bitmap_create(block_cnt);
    ASSERT(swap_bits != NULL);

    lock_init(&lock);
}

static block_sector_t swap_id_to_sector(swapid_t id)
{
    return id * (PGSIZE / BLOCK_SECTOR_SIZE);
}

// read from swap to main memory
// release a swap block
void swap_in(swapid_t id, void *page)
{
    lock_acquire(&lock);
    ASSERT(bitmap_test(swap_bits, id));

    block_sector_t sector = swap_id_to_sector(id);
    size_t size = 0;
    for (; size < PGSIZE; size += BLOCK_SECTOR_SIZE)
        block_read(swap_block, sector++, page);

    bitmap_flip(swap_bits, id);
    lock_release(&lock);
}

// write to swap
// allocate a swap block
swapid_t swap_out(void *page)
{
    lock_acquire(&lock);
    size_t id = bitmap_scan_and_flip(swap_bits, 0, block_cnt, false);
    ASSERT(id != 0);

    block_sector_t sector = swap_id_to_sector(id);
    size_t size = 0;
    for (; size < PGSIZE; size += BLOCK_SECTOR_SIZE)
        block_write(swap_block, sector++, page);

    bitmap_flip(swap_bits, id);
    lock_release(&lock);
    return id;
}
