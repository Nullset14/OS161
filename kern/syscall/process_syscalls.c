#include <types.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/wait.h>
#include <addrspace.h>
#include <synch.h>
#include <current.h>
#include <copyinout.h>
#include <vfs.h>
#include <syscall.h>
#include <kern/process_syscalls.h>
#include <proc.h>
#include <kern/file_syscalls.h>

/* Spawning new process ids */
pid_t
spawn_pid(int *err) {

    int i = PID_MIN;
    while(proc_ids[i] != NULL && i < PID_MAX_256) {
        i++;
    }

    if (i < PID_MAX_256) {
        proc_ids[i] = kmalloc(sizeof(struct proc));

        if(proc_ids[i] == NULL){
            *err = ENOMEM;
            return -1;
        }

        return i;
    }

    *err = EMPROC;
    return -1;
}

pid_t
sys_getpid() {
    return curproc->pid;
}

int
sys_fork(struct trapframe* tf, int *err) {
    struct trapframe* childtf = NULL;
    struct proc* childproc = NULL;
    struct addrspace* childaddr = NULL;
    int result;

    childtf = kmalloc(sizeof(struct trapframe));
    if(childtf == NULL){
        *err = ENOMEM;
        return -1;
    }

    memcpy(childtf, tf, sizeof(struct trapframe));

    result = as_copy(curproc->p_addrspace, &childaddr);
    if(childaddr == NULL){
        kfree(childtf);
        *err = ENOMEM;
        return -1;
    }

    childproc = proc_create_child("child");
    if(childproc == NULL){
        *err = ENOMEM;
        return -1;
    }

    result = thread_fork("process", childproc, child_forkentry, childtf,
                         (unsigned long) childaddr);

    if(result) {
        return result;
    }

    result = childproc->pid;
    curproc->p_numthreads++;
    return result;
}

void
child_forkentry(void *data1, unsigned long data2){

    //(void) data1;
    //(void) data2;

    struct trapframe st_trapframe;
    struct trapframe* childtf = (struct trapframe*) data1;
    struct addrspace* childaddr = (struct addrspace*) data2;

    childtf->tf_v0 = 0;
    childtf->tf_a3 = 0;
    childtf->tf_epc += 4;

    memcpy(&st_trapframe, childtf, sizeof(struct trapframe));
    kfree(childtf);
    childtf = NULL;

    curproc->p_addrspace = childaddr;
    as_activate();

    mips_usermode(&st_trapframe);
};

int
sys_execv(char *progname, char **args) {

    struct addrspace *as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    (void)**args;

    /* Open the file. */
    result = vfs_open(progname, O_RDONLY, 0, &v);
    if (result) {
        return result;
    }

    curproc->p_addrspace = NULL;

    as_destroy(curproc->p_addrspace);

    /* We should be a new process. */
    KASSERT(proc_getas() == NULL);

    /* Create a new address space. */
    as = as_create();
    if (as == NULL) {
        vfs_close(v);
        return ENOMEM;
    }

    /* Switch to it and activate it. */
    proc_setas(as);
    as_activate();

    /* Load the executable. */
    result = load_elf(v, &entrypoint);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        vfs_close(v);
        return result;
    }

    /* Done with the file now. */
    vfs_close(v);

    /* Define the user stack in the address space */
    result = as_define_stack(as, &stackptr);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        return result;
    }

    /* Initialize standard IO */
    std_io_init();

    /* Warp to user mode. */
    enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
                      NULL /*userspace addr of environment*/,
                      stackptr, entrypoint);

    /* enter_new_process does not return. */
    panic("enter_new_process returned\n");
    return EINVAL;
}

pid_t
sys_waitpid(pid_t pid, int *status, int options, int *err) {

    if(status == (int*) 0x0) {
        *err = EFAULT;
        return -1;
    }
    if(status == (int*) 0x40000000 || status == (int*) 0x80000000 || ((int)status & 3) != 0) {
        *err = EFAULT;
        return -1;
    }

    if(options != 0 && options != WNOHANG && options != WUNTRACED){
        *err = EINVAL;
        return -1;
    }

    if(pid < PID_MIN || pid > PID_MAX_256) {
        *err = ESRCH;
        return -1;
    }

    if(curproc->pid != proc_ids[pid]->ppid ){
        *err = ECHILD;
        return -1;
    }

    if(proc_ids[pid] == NULL){
        *err = ESRCH;
        return -1;
    }

    lock_acquire(proc_ids[pid]->exitlock);

    if (proc_ids[pid]->exit_flag == false) {
        if (options == WNOHANG) {
            lock_release(proc_ids[pid]->exitlock);
            return 0;
        }
        else {
            cv_wait(proc_ids[pid]->exitcv, proc_ids[pid]->exitlock);
        }
    }

    *status = proc_ids[pid]->exit_code;

    lock_release(proc_ids[pid]->exitlock);

    for(int fd=3; fd<OPEN_MAX;fd++)
    {
        if(curproc->file_table[fd] != NULL){
            sys_close(fd, err);
            curproc->file_table[fd] = NULL;
        }
    }

    lock_destroy(proc_ids[pid]->exitlock);
    cv_destroy(proc_ids[pid]->exitcv);
    kfree(proc_ids[pid]);
    proc_ids[pid] = NULL;

    return pid;
}

void sys_exit(int exitcode){

    lock_acquire(curproc->exitlock);

    curproc->exit_flag = true;

    int err = 0;

    if(proc_ids[curproc->ppid]->exit_flag == false) {
        curproc->exit_code = _MKWAIT_EXIT(exitcode);

        for(int fd=3;fd<OPEN_MAX;fd++)
        {
            if(curproc->file_table[fd] != NULL){
               sys_close(fd, &err);
                curproc->file_table[fd] = NULL;
            }
        }

        cv_signal(curproc->exitcv, curproc->exitlock);
        lock_release(curproc->exitlock);
    }

    else {
        cv_destroy(curproc->exitcv);
        kfree(proc_ids[curproc->pid]);
        proc_ids[curproc->pid] = NULL;
        lock_destroy(curproc->exitlock);
    }

    thread_exit();
}