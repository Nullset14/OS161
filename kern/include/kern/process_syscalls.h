#ifndef SRC_PROC_SYSCALL_H
#define SRC_PROC_SYSCALL_H

/* Process System Calls */
pid_t spawn_pid(void);

pid_t sys_getpid(void);

pid_t sys_waitpid(pid_t, int *, int, int *);

void sys_exit(int);

int sys_fork(struct trapframe *, int *);

void child_forkentry(void *, unsigned long);

#endif //SRC_PROC_SYSCALL_H
