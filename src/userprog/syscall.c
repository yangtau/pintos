#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
check_user_addr_area(void *uaddr, size_t offset) {
  struct thread *cur = thread_current();
  uint8_t *addr = pg_round_down(uaddr);
  while (addr < (uint8_t*)uaddr + offset) {
    if (!is_user_vaddr(addr) ||
        pagedir_get_page(cur->pagedir, addr) == NULL) {
      thread_exit();
    }
    addr = addr + PGSIZE;
  }
}

static int
get_syscall_num(struct intr_frame *f) {
  // memory int the range should be valid
  check_user_addr_area(f->esp, 4);
  int n = *((int*)f->esp);
  return n;
}

static int
get_syscall_arg(struct intr_frame *f, int n) {
  check_user_addr_area((int32_t*)f->esp + n + 1, 4);
  return *((int*)f->esp + n + 1);
}

static int
sys_write(int fd, const void *buffer, unsigned size) {
  // printf("write fd: %d, buffer: %p, size: %u\n", fd, buffer, size);
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    return 0;
  }
  ASSERT(0);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  switch (get_syscall_num(f))
  {
    case SYS_HALT:
      break;
    case SYS_EXIT:
      thread_current()->ret = get_syscall_arg(f, 0);
      thread_exit();
      break;
    case SYS_EXEC:                   
      break;
    case SYS_WAIT:                   
      break;
    case SYS_CREATE:                 
      break;
    case SYS_REMOVE:                 
      break;
    case SYS_OPEN:                   
      break;
    case SYS_FILESIZE:               
      break;
    case SYS_READ:                   
      break;
    case SYS_WRITE:
      f->eax = sys_write(get_syscall_arg(f, 0),
                         (const void *) get_syscall_arg(f, 1),
                         (unsigned) get_syscall_arg(f, 2));
      break;
    case SYS_SEEK:                   
      break;
    case SYS_TELL:                   
      break;
    case SYS_CLOSE:                  
      break;
    default:
      ASSERT(0);
  }
}
