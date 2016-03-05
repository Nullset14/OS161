#ifndef SRC_FILE_SYSCALL_H
#define SRC_FILE_SYSCALL_H

/* File handle definition */
struct file_handle {

    struct vnode *fh_vnode;
    struct lock *fh_lock;
    mode_t fh_flags;
    int fh_reference_count;
    off_t fh_offset;
};

/* File operation calls */
int sys_open(char *, int, mode_t);

ssize_t sys_read(int, void *, size_t);

ssize_t sys_write(int, void *, size_t);

int sys_close(int);

int std_io_init(void);

#endif //SRC_FILE_SYSCALL_H
