#pragma once

#include "idt.h"

#include <stdint.h>

#define FD_MAX 4
#define FD_PATH_MAX 32
#define FD_KIND_FILE 1
#define FD_KIND_PIPE_R 2
#define FD_KIND_PIPE_W 3

void sched_init(void);
int sched_add_user(uint64_t rip, uint64_t rsp, uint64_t kstack_top, int row,
                   uint64_t user_base, uint64_t cr3);
/* True if this CR3 already has a live mapping at user_base. */
int sched_base_busy(uint64_t user_base, uint64_t cr3);
uint64_t sched_current_cr3(void);
uint64_t sched_current_base(void);
uint64_t sched_current_brk(void);
void sched_set_brk(uint64_t brk);
int sched_row(void);
unsigned sched_note_write(void);
/* Lowest free fd in the current task; stores path leaf/path for later read. */
int sched_fd_open(const char *path);
int sched_fd_close(int fd);
/* Remap oldfd onto newfd. Source stays; target is replaced. Returns newfd or -1. */
int sched_fd_dup2(int oldfd, int newfd);
/* Copy stored path for an open fd into out (FD_PATH_MAX). */
int sched_fd_path(int fd, char *out);
int sched_fd_kind(int fd);
int sched_fd_pipe(int fd);
/* pipe: fill out[0] read and out[1] write. One-page ring. 0 ok, -1 fail. */
int sched_pipe(int out[2]);
/* Kernel-created pipe, not attached to a task. Returns id or -1. */
int sched_pipe_new(void);
/* Free an unused kernel pipe (no remaining refs). */
void sched_pipe_unused(int pipe_id);
/* Bind fd on the last sched_add_user task to a pipe end. Bumps that ref. */
int sched_fd_bind_pipe(int fd, int kind, int pipe_id);
/* Copy from the ring. >=0 bytes, -1 error, -2 empty (block). */
int sched_pipe_read(int fd, void *dst, unsigned n);
/* Copy into the ring. >=0 bytes, -1 error, -2 full (block). */
int sched_pipe_write(int fd, const void *src, unsigned n);
void sched_block_pipe(struct interrupt_frame *frame, int pipe_id);
void sched_on_tick(struct interrupt_frame *frame);
void sched_yield(struct interrupt_frame *frame);
void sched_sleep(struct interrupt_frame *frame, uint64_t ticks);
void sched_wait(struct interrupt_frame *frame);
void sched_exit(struct interrupt_frame *frame);
/* Same slot: new rip/rsp/user_base, keep kstack and CR3, load into frame. */
void sched_reset_current(struct interrupt_frame *frame, uint64_t rip, uint64_t rsp,
                         uint64_t user_base);
/* Copy current task into a free slot (own kstack/CR3/fds). Child rax=0.
 * Returns child slot id, or -1. */
int sched_fork(struct interrupt_frame *frame, uint64_t kstack_top, uint64_t cr3);
