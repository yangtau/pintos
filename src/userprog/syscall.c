#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
check_user_addr_area(const void *uaddr, size_t offset) {
  struct thread *cur = thread_current();
  const uint8_t *addr = pg_round_down(uaddr);
  while (addr < (uint8_t*)uaddr + offset) {
    if (!is_user_vaddr(addr) ||
        pagedir_get_page(cur->pagedir, addr) == NULL) {
      thread_exit();
    }
    addr = addr + PGSIZE;
  }
}

static void check_user_addr_str(const char *str) {
  const char *addr = pg_round_down(str)-PGSIZE;

  while (1) {
    char *t = pg_round_down(str);
    if (t != addr) {
      addr = t;
      check_user_addr_area(addr, PGSIZE);
    }
    if (!*str) break;
    str++;
  }
}

static uint32_t
get_syscall_num(struct intr_frame *f) {
  // memory int the range should be valid
  check_user_addr_area(f->esp, 4);
  return *((uint32_t*)f->esp);
}

static uint32_t
get_syscall_arg(struct intr_frame *f, int n) {
  check_user_addr_area((uint32_t*)f->esp + n + 1, 4);
  return *((uint32_t*)f->esp + n + 1);
}

static int
sys_exec(const char* cmd_line) {
  bool success;

  // check input
  check_user_addr_str(cmd_line);

  tid_t tid = process_execute(cmd_line);
  if (tid == TID_ERROR) return -1;

  // wait for loading
  success = process_wait_for_loading(tid);
  if (!success) {
    ASSERT(process_wait(tid) == -1);
    return -1;
  }

  // successfully loaded
  return (int) tid;
}

static int
sys_wait(tid_t tid) { // map pid to the same tid
  return process_wait(tid);
}

static int
sys_create(const char* file, unsigned size) {
  check_user_addr_str(file);
  return filesys_create(file, size);
}

static int
sys_remove(const char* file) {
  check_user_addr_str(file);
  return filesys_remove(file);
}

static int
sys_open(const char* file_name) {
  struct file* f;
  int ret;

  check_user_addr_str(file_name);

  if ((f = filesys_open(file_name)) == NULL) {
    return -1;
  }

  if ((ret = process_fd_add(f)) == -1) {
    file_close(f);
  }

  return ret;
}

static void 
sys_close(int fd) {
  if (process_fd_get(fd) == NULL) {
    // ingore
    return;
  }
  process_fd_remove(fd);
}

static int
sys_filesize(int fd) {
  struct file *f = process_fd_get(fd);
  if (f == NULL) {
    return -1;
  }

  return file_length(f);
}

// return when get a '\n'
static int
sys_read_from_stdin(uint8_t *buffer, unsigned size) {
  size_t off = 0;
  while (off + 1 < size) {
    uint8_t c = input_getc();
    buffer[off++] = c;
    
    if (c == '\n') break;
  }

  buffer[off++] = '\0';
  return off;
}

static int
sys_read(int fd, void *buffer, unsigned size) {
  struct file *f;

  check_user_addr_area(buffer, size);

  // stdin
  if (fd == STDIN_FILENO) {
    return sys_read_from_stdin(buffer, size);
  }

  if ((f = process_fd_get(fd)) == NULL) {
    return -1;
  }

  return file_read(f, buffer, size);
}

static int
write_to_stdout(const void* buffer, unsigned size) {
  putbuf(buffer, size);
  return size;
}

static int
sys_write(int fd, const void *buffer, unsigned size) {
  struct file *f;
  // check buffer
  check_user_addr_area(buffer, size);

  // printf("write fd: %d, buffer: %p, size: %u\n", fd, buffer, size);
  if (fd == STDOUT_FILENO) {
    return write_to_stdout(buffer, size);
  }

  if ((f = process_fd_get(fd)) == NULL) {
    return -1;
  }

  return file_write(f, buffer, size);
}

static void
sys_seek(int fd, unsigned position) {
  struct file *f;
  if ((f = process_fd_get(fd)) == NULL) {
    return;
  }
  return file_seek(f, position);
}

static unsigned
sys_tell(int fd) {
  struct file *f;
  if ((f = process_fd_get(fd)) == NULL) {
    ASSERT(0);
    return (unsigned)-1;
  }

  return file_tell(f);
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  switch (get_syscall_num(f))
  {
    case SYS_HALT:
      // thread_exit();
      shutdown_power_off();
      break;
    case SYS_EXIT:
      thread_current()->ret = get_syscall_arg(f, 0);
      thread_exit();
      break;
    case SYS_EXEC:
      f->eax = sys_exec((char*)get_syscall_arg(f, 0));
      break;
    case SYS_WAIT:                   
      f->eax = sys_wait((tid_t)get_syscall_arg(f, 0));
      break;
    case SYS_CREATE:
      f->eax = sys_create((char*)get_syscall_arg(f, 0),
                          get_syscall_arg(f, 1));
      break;
    case SYS_REMOVE:
      f->eax = sys_remove((char*)get_syscall_arg(f, 0));
      break;
    case SYS_OPEN:
      f->eax = sys_open((char*)get_syscall_arg(f, 0));
      break;
    case SYS_FILESIZE:
      f->eax = sys_filesize((int)get_syscall_arg(f, 0));
      break;
    case SYS_READ:
      f->eax = sys_read((int)get_syscall_arg(f, 0),
                        (void*)get_syscall_arg(f, 1),
                        (unsigned)get_syscall_arg(f, 2));
      break;
    case SYS_WRITE:
      f->eax = sys_write((int)get_syscall_arg(f, 0),
                         (const void *) get_syscall_arg(f, 1),
                         get_syscall_arg(f, 2));
      break;
    case SYS_SEEK:                   
      sys_seek((int)get_syscall_arg(f, 0), get_syscall_arg(f, 1));
      break;
    case SYS_TELL:
      f->eax = sys_tell((int)get_syscall_arg(f, 0));
      break;
    case SYS_CLOSE:
      sys_close(get_syscall_arg(f, 0));
      break;
    default:
      ASSERT(0);
  }
}
