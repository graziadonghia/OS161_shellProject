
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

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than runprogram() does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <copyinout.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */

int runprogram(char *progname, unsigned long argc, char **args)
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	int i;
	size_t len;
	size_t stack_offset = 0;
	char **argvptr;

	if (argc > ARG_MAX) {
		return E2BIG; 
	}

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result)
	{
		return result;
	}

	/* We should be a new process. */
	KASSERT(proc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as == NULL)
	{
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	proc_setas(as);
	as_activate();

	result = proc_start_filetable("con:", "con:", "con:");
	if(result)
	{
		return result;
	}

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result)
	{
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result)
	{
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}
#if OPT_SHELL
	argvptr = (char **)kmalloc(argc * sizeof(char **));
	if (argvptr == NULL)
	{
		return ENOMEM;
	}

	for (i = 0; i < (int)argc; i++)
	{
		/*allocation*/
		len = strlen(args[i]) + 1;
		argvptr[i] = (char *)kmalloc(sizeof(char *));
		if (argvptr[i] == NULL)
		{
			return ENOMEM;
		}
		stackptr -= len;
		if (stackptr & 0x3)
		{
			stackptr -= stackptr & 0x3;
		}

		/*copy from kernel buffer to user space*/
		copyoutstr(args[i], (userptr_t)stackptr, len, NULL);
		argvptr[i] = (char *)stackptr; /*save current position in the stack of the argument*/
	}
	argvptr[i] = NULL;
	stack_offset += sizeof(char *) * (argc + 1);
	stackptr -= stack_offset;
	result = copyout(argvptr, (userptr_t)stackptr, sizeof(char *) * (argc));
	if (result)
	{
		return result;
	}
#endif
	/* Warp to user mode. */
	enter_new_process(argc, (userptr_t)stackptr /*userspace addr of argv*/,
					  NULL /*userspace addr of environment*/,
					  stackptr, entrypoint);
	panic("enter_new_process returned\n");
	return EINVAL;
}