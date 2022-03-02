
#include <types.h>
#include <kern/unistd.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

#if OPT_SHELL
pid_t sys_getpid(void)
{
	pid_t pid;

	KASSERT(curproc != NULL);
	pid = curproc->p_pid;

	return pid;

}
#endif

