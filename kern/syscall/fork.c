
#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <mips/trapframe.h>
#include <current.h>
#include <synch.h>
#include <array.h>

#include <opt-shell.h>

#if OPT_SHELL

/*
 * Enter user mode for a newly forked process.
 *
 * This function is provided as a reminder. You need to write
 * both it and the code that calls it.
 *
 * Thus, you can trash it and do things another way if you prefer.
 */
void enter_forked_process(struct trapframe *tf)
{

	// Duplicate frame so it's on stack (now it's in the heap)
	struct trapframe forkedTf = *tf; // copy trap frame onto kernel stack
	forkedTf.tf_v0 = 0;				 // return value is 0
	forkedTf.tf_a3 = 0;				 // return with success

	forkedTf.tf_epc += 4; // return to next instruction

	/*activate the addrspace*/
	as_activate();

	mips_usermode(&forkedTf);
}

/*
 * Caller function for enter_forked_process
 */

static void
call_enter_forked_process(void *tfv)
{
	struct trapframe *tf = (struct trapframe *)tfv;
	enter_forked_process(tf);

	panic("enter_forked_process returned (should not happen)\n");
}

/*
 * Function that adds the child process to the children array 
 * of the parent process (struct array)
 */
static void
proc_add_child(struct proc *parent, struct proc *child)
{

	KASSERT(parent != NULL);
	KASSERT(child != NULL);
	unsigned index_ret = (unsigned)child->p_pid;
	array_add(parent->p_children, (void *)child, &index_ret); /*potrei avere un errore*/
}
/**
 * Implementation of the fork system call.
 * 
 * Parameters:
 * - ctf: pointer to parent's trapframe
 * - retval: pointer to return value
 
 * Return value:
 * - Parent returns with child's pid
 * - Child returns with 0
 */
int sys_fork(struct trapframe *ctf, pid_t *retval)
{
	struct trapframe *tf_child;
	struct proc *child;
	int result;
	int new_pid = 0;
	struct proc *parent = curproc;
	struct thread *thread = curthread;

	KASSERT(thread != NULL);
	KASSERT(parent != NULL);
	if (curproc->p_pid + 1 > MAX_PROC)
	{
		*retval = ENPROC;
		return -1; //too many processes in the system
	}

	/*creation of child proc struct*/
	child = proc_create_runprogram(parent->p_name);
	if (child == NULL)
	{
		*retval = ENOMEM;
		return -1;
	}
	new_pid = child->p_pid;

	/*check if generated pid is valid*/
	if (new_pid < PID_MIN || new_pid > PID_MAX)
	{
		*retval = EINVAL;
		proc_destroy(child);
		return -1;
	}

	tf_child = kmalloc(sizeof(struct trapframe));
	if (tf_child == NULL)
	{
		*retval = ENOMEM;
		proc_destroy(child);
		return -1;
	}

	/*copy of parent's trapframe*/
	memcpy(tf_child, ctf, sizeof(struct trapframe));

	/*copy of parent's address space*/
	result = as_copy(curproc->p_addrspace, &(child->p_addrspace));
	if (result || child->p_addrspace == NULL)
	{
		*retval = -1;
		proc_destroy(child);
		return -result;
	}

	/*child points to parent*/
	child->p_parent = parent;

	proc_add_child(parent, child);

	/*file table copied inside thread_fork in mutual exclusion*/
	result = thread_fork(
		curthread->t_name, child,
		(void *)call_enter_forked_process,
		(void *)tf_child, (unsigned long)0);

	if (result)
	{
		*retval = ENOMEM;
		proc_destroy(child);
		kfree(tf_child);
		return -1;
	}

	*retval = child->p_pid; /*parent returns with child's pid immediately*/
	return 0;
}

#endif