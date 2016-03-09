#include <types.h>
#include <syscall.h>
#include <synch.h>
#include <vfs.h>
#include <current.h>
#include <proc.h>
#include <limits.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/file_syscalls.h>
#include <uio.h>
#include <kern/iovec.h>
#include <copyinout.h>
#include <vnode.h>
#include <kern/seek.h>
#include <kern/stat.h>

/* File Open System Call */
int
sys_open(char *filename, int flags, mode_t mode, int *err)
{

    /* Add Validations Here*/

    int response = 0;
    int fd = 3;

    for (;fd < OPEN_MAX; fd++) {

        if (curproc->file_table[fd] != NULL) {
            continue;
        }

        /* Claim a file descriptor */
        curproc->file_table[fd] = kmalloc(sizeof(struct file_handle));
        if (curproc->file_table[fd] == NULL) {
            kfree(curproc->file_table);
            *err = ENOMEM;
            return -1;
        }

        curproc->file_table[fd]->fh_lock = lock_create("fd_lock");
        if (curproc->file_table[fd] == NULL) {
            kfree(curproc->file_table);
            *err = ENOMEM;
            return -1;
        }

        curproc->file_table[fd]->fh_offset = 0;
        curproc->file_table[fd]->fh_flags = flags;
        curproc->file_table[fd]->fh_reference_count = 1;

        response = vfs_open(filename, flags, mode, &(curproc->file_table[fd]->fh_vnode));

        if (response) {
            *err = response;
            return -1;
        }

        break;
    }

    if (fd >= OPEN_MAX) {
        *err = EMFILE;
        return -1;
    }

    return fd;
}

/* File Read System Call */
ssize_t
sys_read(int fd, void *buf, size_t buflen, int *err) {

    /* Validations */
    if (curproc->file_table[fd] == NULL) {
        *err = ENOENT;
        return -1;
    }

    if (buf == NULL) {
        *err = EFAULT;
        return -1;
    }

    if (curproc->file_table[fd]->fh_flags & O_WRONLY) {
        *err = EACCES;
        return -1;
    }

    if (buflen <= 0) {
        *err = EINVAL;
        return -1;
    }

    if (fd < 0 || fd > OPEN_MAX) {
        *err = EBADF;
        return -1;
    }

    int response = 0;

    struct uio read_uio;
    struct iovec read_iovec;

    /* data and length */
    read_iovec.iov_ubase = buf;
    read_iovec.iov_len = buflen;

    /* flags and references */
    read_uio.uio_iovcnt = 1;
    read_uio.uio_iov = &read_iovec;
    read_uio.uio_segflg = UIO_USERSPACE;
    read_uio.uio_rw = UIO_READ;
    read_uio.uio_space = curproc->p_addrspace;
    read_uio.uio_resid = buflen;

    /* Lock file descriptor */
    lock_acquire(curproc->file_table[fd]->fh_lock);

    read_uio.uio_offset = curproc->file_table[fd]->fh_offset;
    int residual = read_uio.uio_resid;

    response = VOP_READ(curproc->file_table[fd]->fh_vnode, &read_uio);

    /* uio_resid will have been decremented by the amount transferred */
    residual -= read_uio.uio_resid;

    /* Update offset in the current process */
    curproc->file_table[fd]->fh_offset = read_uio.uio_offset;

    /* Release lock file descriptor */
    lock_release(curproc->file_table[fd]->fh_lock);

    if (response) {
        *err = response;
        return -1;
    }

    return residual;
}

/* File Write System Call */
ssize_t
sys_write(int fd, void *buf, size_t buflen, int *err) {

    /* Validations */
    if (curproc->file_table[fd] == NULL) {
        *err = ENOENT;
        return -1;
    }

    if (buf == NULL) {
        *err = EFAULT;
        return -1;
    }

    if (curproc->file_table[fd]->fh_flags & O_RDONLY) {
        *err = EACCES;
        return -1;
    }

    if (buflen <= 0) {
        *err = EINVAL;
        return -1;
    }

    if (fd < 0 || fd > OPEN_MAX) {
        *err = EBADF;
        return -1;
    }

    int response = 0;

    struct uio write_uio;
    struct iovec write_iovec;

    /* data and length */
    write_iovec.iov_ubase = buf;
    write_iovec.iov_len = buflen;

    /* flags and references */
    write_uio.uio_iovcnt = 1;
    write_uio.uio_iov = &write_iovec;
    write_uio.uio_segflg = UIO_USERSPACE;
    write_uio.uio_rw = UIO_WRITE;
    write_uio.uio_space = curproc->p_addrspace;
    write_uio.uio_resid = buflen;

    /* Lock file descriptor */
    lock_acquire(curproc->file_table[fd]->fh_lock);

    write_uio.uio_offset = curproc->file_table[fd]->fh_offset;
    int residual = write_uio.uio_resid;

    response = VOP_WRITE(curproc->file_table[fd]->fh_vnode, &write_uio);

    /* uio_resid will have been decremented by the amount transferred */
    residual = residual - write_uio.uio_resid;

    /* Update offset in the current process */
    curproc->file_table[fd]->fh_offset = write_uio.uio_offset;

    /* Release lock file descriptor */
    lock_release(curproc->file_table[fd]->fh_lock);

    if (response) {
        *err = response;
        return -1;
    }

    return residual;
}

/* File close System Call */
int sys_close(int fd, int *err) {

    /* Validations */

    if (fd < 0 || fd > OPEN_MAX) {
        *err = EBADF;
        return -1;
    }

    if (curproc->file_table[fd] == NULL) {
        *err = ENOENT;
        return -1;
    }

    /* Lock file descriptor */
    lock_acquire(curproc->file_table[fd]->fh_lock);

    curproc->file_table[fd]->fh_reference_count--;

    /* Release file descriptor */
    if (curproc->file_table[fd]->fh_reference_count == 0) {
        vfs_close(curproc->file_table[fd]->fh_vnode);

        lock_release(curproc->file_table[fd]->fh_lock);
        lock_destroy(curproc->file_table[fd]->fh_lock);

        curproc->file_table[fd] = NULL;
        kfree(curproc->file_table[fd]);

        return 0;
    }

    /* Release lock file descriptor */
    lock_release(curproc->file_table[fd]->fh_lock);

    return 0;
}

/* File dup system call */
int
sys_dup2(int oldfd, int newfd, int *err) {

    /* Validations */

    if (oldfd < 0 || newfd > OPEN_MAX) {
        *err = EBADF;
        return -1;
    }

    if (newfd < 0 || newfd > OPEN_MAX) {
        *err = EBADF;
        return -1;
    }

    if (curproc->file_table[oldfd] == NULL) {
        *err = EBADF;
        return -1;
    }

    if (curproc->file_table[newfd] == NULL) {
        *err = EBADF;
        return -1;
    }

    int response = 0;

    int high, low;

    /* To avoid deadlock */
    if (newfd > oldfd) {
        high = newfd;
        low  = oldfd;
    } else if (oldfd > newfd) {
        high = oldfd;
        low = newfd;
    } else {
        *err = EBADF;
        return -1;
    }

    lock_acquire(curproc->file_table[high]->fh_lock);
    lock_acquire(curproc->file_table[low]->fh_lock);

    response = sys_close(newfd, err);

    if (response == 0) {
        curproc->file_table[newfd] = curproc->file_table[oldfd];
        curproc->file_table[oldfd]->fh_reference_count++;
    }

    lock_acquire(curproc->file_table[low]->fh_lock);
    lock_release(curproc->file_table[high]->fh_lock);

    if (response) {
        return -1;
    }

    return newfd;

}

int
sys_chdir(char *pathname, int *err) {

    /* Validations */

    if (pathname == NULL) {
        *err = EFAULT;
        return -1;
    }

    int response = vfs_chdir(pathname);

    if (response) {
        *err = response;
        return -1;
    }

    return 0;
}

int
sys___getcwd(char *buf, size_t buflen, int *err) {

    /* Validations */

    if (buflen <= 0) {
        *err = EINVAL;
        return -1;
    }

    if (buf == NULL) {
        *err = EFAULT;
        return -1;
    }

    struct uio cwd_uio;
    struct iovec cwd_iovec;

    /* data and length */
    cwd_iovec.iov_ubase = (void *)buf;
    cwd_iovec.iov_len = buflen;

    /* flags and references */
    cwd_uio.uio_iovcnt = 1;
    cwd_uio.uio_iov = &cwd_iovec;
    cwd_uio.uio_segflg = UIO_USERSPACE;
    cwd_uio.uio_rw = UIO_READ;
    cwd_uio.uio_space = curproc->p_addrspace;
    cwd_uio.uio_resid = buflen;

    int residual = cwd_uio.uio_resid;
    int response = vfs_getcwd(&cwd_uio);

    if (response) {
        *err = response;
        return -1;
    }

    residual -= cwd_uio.uio_resid;

    return residual;
}

off_t
sys_lseek(int fd, off_t pos, int whence, int *err) {

    /* Validations */

    if (fd < 0 || fd > OPEN_MAX) {
        *err = EBADF;
        return -1;
    }

    if (pos < 0) {
        *err = EINVAL;
        return -1;
    }

    if (curproc->file_table[fd] == NULL) {
        *err = EBADF;
        return -1;
    }

    int new_position = -1;

    switch(whence) {

        /* the new position is pos */
        case SEEK_SET:
            lock_acquire(curproc->file_table[fd]->fh_lock);

            curproc->file_table[fd]->fh_offset = pos;
            new_position = curproc->file_table[fd]->fh_offset;

            if (!VOP_ISSEEKABLE(curproc->file_table[fd]->fh_vnode)) {
                *err = EINVAL;
                return -1;
            }

            lock_release(curproc->file_table[fd]->fh_lock);
            break;

        /* the new position is the current position plus pos */
        case SEEK_CUR:
            lock_acquire(curproc->file_table[fd]->fh_lock);

            curproc->file_table[fd]->fh_offset += pos;
            new_position = curproc->file_table[fd]->fh_offset;

            if (!VOP_ISSEEKABLE(curproc->file_table[fd]->fh_vnode)) {
                *err = EINVAL;
                return -1;
            }

            lock_release(curproc->file_table[fd]->fh_lock);
            break;

        /* the new position is the position of end-of-file plus pos */
        case SEEK_END:
            lock_acquire(curproc->file_table[fd]->fh_lock);

            struct stat statbuf;
            int response = VOP_STAT(curproc->file_table[fd]->fh_vnode, &statbuf);
            if (response) {
                *err = response;
                lock_release(curproc->file_table[fd]->fh_lock);
                return -1;
            }

            curproc->file_table[fd]->fh_offset = pos +   statbuf.st_size;

            if (!VOP_ISSEEKABLE(curproc->file_table[fd]->fh_vnode)) {
                *err = EINVAL;
                return -1;
            }

            new_position = curproc->file_table[fd]->fh_offset;

            lock_release(curproc->file_table[fd]->fh_lock);
            break;

        default:
            *err = EINVAL;
            return -1;
    }

    return new_position;
}

/* Initialize console file descriptors */
int
std_io_init() {

    int console_fd = 0;
    for(; console_fd < 3; console_fd++) {

        char io[] = "con:";

        curproc->file_table[console_fd] = kmalloc(sizeof(struct file_handle));
        if (curproc->file_table[console_fd] == NULL) {
            return ENOMEM;
        }

        curproc->file_table[console_fd]->fh_offset = 0;
        curproc->file_table[console_fd]->fh_reference_count = 1;

        int response = 0;

        switch(console_fd) {
            case 0  :

                curproc->file_table[console_fd]->fh_flags = O_RDONLY;
                response = vfs_open(io, O_RDONLY, 0664, &(curproc->file_table[console_fd]->fh_vnode));

                curproc->file_table[console_fd]->fh_lock = lock_create("read");
                if (curproc->file_table[console_fd]->fh_lock == NULL) {
                    response = ENOMEM;
                }

                if (response) {

                    lock_destroy(curproc->file_table[0]->fh_lock);
                    vfs_close(curproc->file_table[0]->fh_vnode);
                    kfree(curproc->file_table[0]);

                    return response;
                }

                break;

            case 1  :

                curproc->file_table[console_fd]->fh_flags = O_WRONLY;
                response = vfs_open(io, O_WRONLY, 0664, &(curproc->file_table[console_fd]->fh_vnode));

                curproc->file_table[console_fd]->fh_lock = lock_create("write");
                if (curproc->file_table[console_fd]->fh_lock == NULL) {
                    response = ENOMEM;
                }

                if (response) {

                    lock_destroy(curproc->file_table[0]->fh_lock);
                    lock_destroy(curproc->file_table[1]->fh_lock);

                    vfs_close(curproc->file_table[0]->fh_vnode);
                    vfs_close(curproc->file_table[1]->fh_vnode);

                    kfree(curproc->file_table[0]);
                    kfree(curproc->file_table[1]);

                    return response;
                }

                break;

            case 2 :

                curproc->file_table[console_fd]->fh_flags = O_WRONLY;
                response = vfs_open(io, O_WRONLY, 0664, &(curproc->file_table[console_fd]->fh_vnode));

                curproc->file_table[console_fd]->fh_lock = lock_create("error");
                if (curproc->file_table[console_fd]->fh_lock == NULL) {
                    response = ENOMEM;
                }

                if (response) {

                    lock_destroy(curproc->file_table[0]->fh_lock);
                    lock_destroy(curproc->file_table[1]->fh_lock);
                    lock_destroy(curproc->file_table[2]->fh_lock);

                    vfs_close(curproc->file_table[0]->fh_vnode);
                    vfs_close(curproc->file_table[1]->fh_vnode);
                    vfs_close(curproc->file_table[2]->fh_vnode);

                    kfree(curproc->file_table[0]);
                    kfree(curproc->file_table[1]);
                    kfree(curproc->file_table[2]);

                    return response;
                }

                break;
        }

    }

    return 0;
}