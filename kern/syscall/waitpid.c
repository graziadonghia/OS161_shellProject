#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <kern/wait.h>
#include <copyinout.h>

#include <opt-shell.h>

#if OPT_SHELL    

int sys_waitpid(pid_t pid, userptr_t statusp, int options, pid_t *err)
{
    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL);
    int result;
    /*status pointer check invalid alignment*/
    if ((unsigned int)statusp & 0x3)
    {
        *err = EFAULT;
        return -1;
    }

    /*invalid ptr*/
    if ((void *)statusp == INVALID_PTR)
    {
        *err = EFAULT;
        return -1;
    }
    /*status must point to userland*/
    if ((void *)statusp >= KERNEL_PTR)
    {
        *err = EFAULT;
        return -1;
    }

    /*options check*/
    if (options != 0 && options != WNOHANG)
    {
        *err = EINVAL;
        return -1;
    }
    //invalid pid check
    if (pid <= 0)
    {
        *err = ESRCH;
        return -1;
    }
    //get the process associated with the given pid
    struct proc *p = proc_search_pid(pid, err);
    int s;

    if (p == NULL)
    {
        //the pid doesn't exist
        *err = ESRCH; //the pid argument named a nonexistent process
        return -1; 
    }
    if (pid > PID_MAX || pid < PID_MIN)
    {
        *err = EINVAL;
        return -1;
    }
    //a process cannot wait for itself
    if (p == curproc)
    {
        *err = EPERM;
        return -1; //operation not permitted
    }
    //a process cannot wait for its parent
    if (p == curproc->p_parent)
    {
        *err = EPERM;
        return -1;
    }
    //if the pid exists, are we allowed to wait for it? i.e, is it our child?
    if (curproc != p->p_parent)
    {
        *err = ECHILD;
        return -1;
    }
    //sys_waitpid returns error if the calling process doesn't have any child
    if (curproc->p_children->num == 0)
    {
        *err = ECHILD;
        return -1;
    }
    //siblings cannot wait for each other
    if (p->p_parent == curproc->p_parent) {
        *err = EPERM;
        return -1;
    }
    //if WNOHANG was given, and said process is not yet dead, we immediately return 0
    if (options == WNOHANG && !p->p_exited)
    {
        *err = 0;
        return 0;
    }
    s = proc_wait(p);
    /*null status pointer*/
    if (statusp != NULL)
    {   
        /*copy onto user space the return status*/
        result = copyout(&s, statusp, sizeof(int));
        if (result) {
            *err = -1;
            return result;
        }

    }

    return p->p_pid;

}

#endif

