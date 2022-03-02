#include <kern/types.h>
#include <types.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <vfs.h>
#include <uio.h>
#include <kern/iovec.h>
#include <current.h>
#include <copyinout.h>
#include <proc.h>

#include <opt-shell.h>

#if OPT_SHELL

int dir_parser(const char *);
void uspace_uio_kinit(struct uio *, struct iovec *, userptr_t, size_t);

/**
 * getcwd: get the current working directory
 * 
 * Parameters:
 * - buf: pointer to the buffer which will store the cwd
 * - size: size of the buffer
 * - err: this contains
 *      -> -1 on failure
 *      -> the amount of data read on success
 * 
 * Return value:
 * - the size of the string read, if the operation has been successfully completed
 * - -1, if there's been an error (see err for the error code)
 */
int sys___getcwd(userptr_t buf, size_t size, int *err)
{
    struct uio uio;
    struct iovec iovec;
    int vfs_err;

    /* Checks whether the buf is NULL */
    if ((void *)buf == NULL)
    {
        *err = EFAULT;
        return -1;
    }

    /* Checks whether buf address is 0x40000000 or greater than 0x80000000 (user space) */
    if ((void *)buf == INVALID_PTR || (void *)buf >= KERNEL_PTR)
    {
        *err = EFAULT;
        return -1;
    }

    /* 
        This could be a useful check for whether the 
        current working director is a NULL pointer
    */
    struct vnode *cwd_vn = curproc->p_cwd;
    if (cwd_vn == 0x00)
    {
        *err = ENOTDIR;
        return -1;
    }

    /* Setting the user space without involving the kernel level address space */
    uspace_uio_kinit(&uio, &iovec, buf, size);

    vfs_err = vfs_getcwd(&uio);
    if (vfs_err != 0)
    {
        *err = vfs_err;
        return -1;
    }

    *err = 0;

    /**
     * uio.uio_resid contains the 
     * "remaining amt of data to xfer", so
     * size - uio.uio_resid is equal to the
     * amount of read data.
     */
    return size - uio.uio_resid;
}

/**
 * chdir: change the current working directory
 * 
 * Parameters:
 * - path: the new path to which the cwd has to be set
 * 
 * Return value:
 * - 0, if success
 * - -1, otherwise
 */
int sys_chdir(userptr_t path, int *err)
{
    int vfs_err;
    char *kpath = NULL;

    /* Checks whether the path is NULL */
    if ((void *)path == NULL)
    {
        *err = EFAULT;
        return -1;
    }

    /* Checks whether path address is 0x40000000 or greater than 0x80000000 (user space) */
    if ((void *)path == INVALID_PTR || (void *)path >= KERNEL_PTR)
    {
        *err = EFAULT;
        return -1;
    }

    /* Check whether the new dir path is valid or not */
    if (dir_parser((const char *)path))
    {
        *err = ENOTDIR;
        return -1;
    }

    kpath = (char *)kmalloc(__PATH_MAX);

    if (kpath == 0x00)
    {
        // bad allocation
        *err = EFAULT;
        return -1;
    }

    if (copyinstr(path, kpath, __PATH_MAX, NULL))
    {
        kfree(kpath);
        *err = EFAULT;
        return -1;
    }

    vfs_err = vfs_chdir(kpath);

    kfree(kpath);
    if (vfs_err)
    {
        *err = vfs_err;
        return -1;
    }

    return 0;
}

/**
 * Directory parser.
 * Checks whether the new_path string
 * represent a valid path from a 
 * syntactical point of view;
 * 
 * Parameter:
 * - dir: string containing the new path
 * 
 * Return value:
 * - 0: success
 * - 1: bad path
 * - 2: memory allocation failed
 */
int dir_parser(const char *dir)
{
    char *l1_dir = (char *)kmalloc(sizeof(dir));
    char *_tmp_free_ptr;
    char prev = '\0';

    if (l1_dir == NULL)
        return -2;

    strcpy(l1_dir, dir);
    _tmp_free_ptr = l1_dir;

    /* First-layer parser */
    for (; *l1_dir != '\0'; l1_dir++)
    {
        if (prev == '\0')
        {
            prev = *l1_dir;
            continue;
        }
        if (*l1_dir == '/' && prev == '/')
            return 1;
        prev = *l1_dir;
    }

    /* Free */
    kfree(_tmp_free_ptr);

    return 0;
}

/**
 * uspace_uio_kinit
 * 
 * Sets the user space without 
 * involving the kernel level
 * address space.
 */
void uspace_uio_kinit(struct uio *uio, struct iovec *iovec, userptr_t buf, size_t size)
{
    uio_kinit(iovec, uio, buf, size, 0, UIO_READ);
    iovec->iov_ubase = buf;
    uio->uio_segflg = UIO_USERSPACE;
    uio->uio_space = proc_getas();
}

#endif