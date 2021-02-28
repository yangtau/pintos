#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/file.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

bool process_wait_for_loading (tid_t);

// file descriptor
int process_fd_add(struct file* f);
void process_fd_remove(int fd);
struct file* process_fd_get(int fd);

#endif /* userprog/process.h */
