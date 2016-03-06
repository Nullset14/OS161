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
int sys_open(char *, int, mode_t, int *);

ssize_t sys_read(int, void *, size_t, int *);

ssize_t sys_write(int, void *, size_t, int *);

int sys_close(int, int *);

int sys_dup2(int, int, int *);

int sys_chdir(char *, int *);

int sys___getcwd(char *, size_t, int *);

int std_io_init(void);

off_t sys_lseek(int, off_t, int, int *);

#endif //SRC_FILE_SYSCALL_H
