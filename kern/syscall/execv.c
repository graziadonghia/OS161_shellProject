
#include <types.h>
#include <lib.h>
#include <copyinout.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <kern/fcntl.h>
#include <limits.h>
#include <kern/errno.h>
#include <machine/trapframe.h>
#include <synch.h>
#include <vfs.h>
#include <syscall.h>

#include <opt-shell.h>

#if OPT_SHELL

/*Shared data structures --> we need a lock for them (exec_lock)*/
static char karg[ARG_MAX];			/*argument string, it's not a string array*/
static unsigned char kbuf[ARG_MAX]; /*array of bytes*/

/*
 * This function returns the required padding for user args
 */
static int
padded_length(char *arg, int alignment)
{
	int len = strlen(arg) + 1;

	if (len % 4 == 0)
		return len;

	return len + (alignment - (len % alignment));
}

/*
 * This function copies the args from user space to kernel space, 
 * building the kernel buffer, which will be then used to store args pointers
 * for user stack
 */
static int
copy_args_to_kbuf(char **args, int argc, int *buflen)
{
	int i;
	int err;
	int padding = 0;
	unsigned char *p_begin = kbuf;
	int last_offset = argc * sizeof(char *);
	unsigned char *p_end = p_begin + last_offset;
	volatile int offset;
	*buflen = 0;

	for (i = 0; i < argc - 1; i++)
	{
		err = copyinstr((userptr_t)args[i], karg, sizeof(karg), NULL);
		if (err)
			return err;

		offset = last_offset + padding;
		padding = padded_length(karg, 4);

		memcpy(p_end, karg, padding);

		*p_begin = offset;

		p_end += padding;

		p_begin += sizeof(char *);

		last_offset = offset;

		*buflen += padding + sizeof(char *); /*how much will the stackptr have to shift for every arg*/
	}

	*buflen += sizeof(char *);
	return 0;
}

/*
 * This function modifies the kbuf content in order to store
 * user stack position of the passed arguments
 */
static int
change_kbuf_for_userstack(int argc, vaddr_t stackptr)
{
	int i;
	int new_offset = 0;
	int old_offset = 0;
	int index;

	for (i = 0; i < argc - 1; ++i)
	{
		index = i * sizeof(char *); //position of the i-th argument
		//read the old offset.
		old_offset = kbuf[index];

		//calculate the new offset
		new_offset = stackptr + old_offset;

		//store it instead of the old one.
		memcpy(kbuf + index, &new_offset, sizeof(int));
	}

	return 0;
}

/**
 * Implementation of the execv system call.
 * 
 * Parameters:
 * - program: string with user program name
 * - args: array of args
 */
int sys_execv(char *program, char **args)
{
	struct addrspace *newas;
	struct addrspace *oldas;
	struct vnode *vn;
	vaddr_t entrypoint;
	vaddr_t stackptr;
	int err;
	char *kprogram;
	int argc;
	int buflen;
	int len, i;

	//preliminar checks
	KASSERT(curproc != NULL);
	if (program == NULL || args == NULL)
	{
		return EFAULT;
	}

	if ((void *)program == INVALID_PTR || (void *)args == INVALID_PTR)
	{
		return EFAULT;
	}
	if ((void *)program >= KERNEL_PTR || (void *)args >= KERNEL_PTR)
	{
		return EFAULT;
	}
	lock_acquire(exec_lock);

	/*find how many arguments*/
	for (i = 0; args[i] != NULL; i++)
		;
	argc = i + 1; //last NULL 
	if (argc > ARG_MAX) {
		return E2BIG;
	}
	err = copy_args_to_kbuf(args, argc, &buflen);
	if (err)
	{
		lock_release(exec_lock);
		return err;
	}

	len = strlen(program) + 1;
	kprogram = kmalloc(len);
	if (kprogram == NULL)
	{
		lock_release(exec_lock);
		return ENOMEM;
	}
	err = copyinstr((userptr_t)program, kprogram, len, NULL);
	if (err)
	{
		lock_release(exec_lock);
		return err;
	}

	//open the given executable.
	err = vfs_open(kprogram, O_RDONLY, 0, &vn);
	if (err)
	{
		lock_release(exec_lock);
		return err;
	}

	//create the new addrspace.
	newas = as_create();
	if (newas == NULL)
	{
		lock_release(exec_lock);
		vfs_close(vn);
		return ENOMEM;
	}

	//activate the new addrspace.
	oldas = proc_setas(newas); //proc_setas returns the old addrspace
	as_activate();

	err = proc_start_filetable("con:", "con:", "con:");
	if(err)
	{
		return err;
	}
	//load the elf executable.
	err = load_elf(vn, &entrypoint);
	if (err)
	{
		proc_setas(oldas);
		as_activate();

		as_destroy(newas);
		vfs_close(vn);
		lock_release(exec_lock);
		return err;
	}

	//create a stack for the new addrspace.
	err = as_define_stack(newas, &stackptr);
	if (err)
	{
		proc_setas(oldas);
		as_activate();

		as_destroy(newas);
		vfs_close(vn);
		lock_release(exec_lock);
		return err;
	}

	//adjust the stackptr to reflect the change
	/*stackptr starts from 0x80000000,
	 *i.e if I lower by buflen I go to the user stack
	 */
	stackptr -= buflen;
	err = change_kbuf_for_userstack(argc, stackptr);
	if (err)
	{
		proc_setas(oldas);
		as_activate();

		as_destroy(newas);
		vfs_close(vn);
		lock_release(exec_lock);
		return err;
	}

	//copy the arguments into the new user stack.
	err = copyout((void *)kbuf, (userptr_t)stackptr, buflen);
	if (err)
	{
		proc_setas(oldas);
		as_activate();
		as_destroy(newas);
		vfs_close(vn);
		lock_release(exec_lock);
		return err;
	}

	lock_release(exec_lock);

	vfs_close(vn);

	as_destroy(oldas);

	//"flush" kbuf for next invocation
	for (i = 0; i < ARG_MAX; i++) {
		kbuf[i] = 0x0;
	}
	enter_new_process(argc - 1, (userptr_t)stackptr, NULL, stackptr, entrypoint);

	panic("execv: enter_new_process should not return.");
	return EINVAL;
}

#endif