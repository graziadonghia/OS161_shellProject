
#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <spl.h>
#include <vnode.h>
#include <vfs.h>
#include <test.h>
#include <kern/wait.h>

#include <opt-shell.h>

#if OPT_SHELL

static int 
close_all_files(struct proc *p) {
    int i, err;
    for (i = 0; i < OPEN_MAX; i++)
    {
        if (p->fileTable[i] != NULL)
        {
            if (sys_close(i, &err)) return -1;
        }
    }
    return 0;
}
void sys__exit(int status)
{
    struct proc *p = curproc;
    int err;

    //close all open files
    err = close_all_files(p);
    if (err) {
        panic("Problem closing open file of curproc.\n");
    }
    p->p_status = status & 0xff; /* just lower 8 bits returned */

    proc_remthread(curthread);
#if USE_SEMAPHORE_FOR_WAITPID
    p->p_exited = true;
    V(p->p_sem);
#else
    lock_acquire(p->p_cv_lock);
    p->p_exited = true; /*condition*/
    cv_signal(p->p_cv, p->p_cv_lock);
    lock_release(p->p_cv_lock);
#endif 

    thread_exit();

    panic("thread_exit returned (should not happen)\n");
    (void)status;
}

#endif