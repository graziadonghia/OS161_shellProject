/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYSCALL_H_
#define _SYSCALL_H_


#include <cdefs.h> /* for __DEAD */

#include <opt-shell.h>

/*constants for pointer check in system calls*/
#define KERNEL_PTR ((void *)0x80000000)
#define INVALID_PTR ((void *)0x40000000)

struct trapframe; /* from <machine/trapframe.h> */

/*
 * The system call dispatcher.
 */

void syscall(struct trapframe *tf);

/*
 * Support functions.
 */

/* Helper for fork(). You write this. */
void enter_forked_process(struct trapframe *tf);

/* Enter user mode. Does not return. */
__DEAD void enter_new_process(int argc, userptr_t argv, userptr_t env,
		       vaddr_t stackptr, vaddr_t entrypoint);


/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

struct openfile;
int sys_reboot(int code);
int sys___time(userptr_t user_seconds, userptr_t user_nanoseconds);
void openfileIncrRefCount(struct openfile *of);

#if OPT_SHELL
/**
 * curdir_syscalls.c
 */
int sys___getcwd(userptr_t buf, size_t size, int *ret_val);
int sys_chdir(userptr_t path, int *return_value);
#endif

#if OPT_SHELL
/**
 * file_syscalls.c
 */
int sys_open(userptr_t path, int openflags, mode_t mode, int *err);
int sys_close(int fd, int *err);
int sys_write(int fd, userptr_t buf_ptr, size_t size, int *err);
int sys_read(int fd, userptr_t buf_ptr, size_t size, int *err);
int sys_dup2(int old_fd, int new_fd, int *err);
off_t sys_lseek(int fd, off_t offset, int whence, int *err);
int sys_fstat(int fd, userptr_t buf, int *err);
int sys_mkdir(userptr_t pathname, mode_t mode, int *err);
#endif

#if OPT_SHELL
/**
 * fork.c
 */
int sys_fork(struct trapframe *ctf, pid_t *retval);
#endif

#if OPT_SHELL
/**
 * execv.c
 */
int sys_execv(char *program, char ** args);
#endif

/**
 * getpid.c
 */
pid_t sys_getpid(void);

#if OPT_SHELL
/**
 * exit.c
 */
void sys__exit(int status);
#endif

#if OPT_SHELL
/**
 * waitpid.c
 */
int sys_waitpid(pid_t pid, userptr_t statusp, int options, pid_t *err);
#endif

int proc_start_filetable(const char *strin, const char *strout, const char *strerr);

#endif /* _SYSCALL_H_ */
