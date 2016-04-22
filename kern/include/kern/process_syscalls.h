#ifndef SRC_PROC_SYSCALL_H
#define SRC_PROC_SYSCALL_H

/* Process System Calls */
pid_t spawn_pid(struct proc *, int *);

pid_t sys_getpid(void);

int sys_execv(char *, char **, int *);

int sys_fork(struct trapframe *, int *);

void child_forkentry(void *, unsigned long);

pid_t sys_waitpid(pid_t, int *, int, int *);

void sys_exit(int, bool);

void *sys_sbrk(intptr_t, int *);

#endif //SRC_PROC_SYSCALL_H
