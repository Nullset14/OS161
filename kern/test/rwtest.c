/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/secret.h>
#include <kern/test161.h>
#include <spinlock.h>

#define CREATELOOPS 2
#define NSEMLOOPS   2
#define NLOCKLOOPS  2
#define NRWLOOPS    2
#define NTHREADS    24

static struct rwlock *testrwlock = NULL;

struct spinlock status_lock;
static bool test_status = TEST161_FAIL;


int rwtest(int nargs, char **args) {

	(void)nargs;
	(void)args;

	kprintf_n("Starting  rwt1...\n");
    testrwlock = rwlock_create("testrwlock");
    if(testrwlock == NULL) {
        panic("rwt1: rwlock_create failed!");
    }

	spinlock_init(&status_lock);

    kprintf_n("This shoud panic on success!");
    rwlock_release_read(testrwlock);

	rwlock_destroy(testrwlock);
	rwlock_destroy(testrwlock);
    testrwlock = NULL;

	kprintf_t("\n");
	success(test_status, SECRET, "rwt1");

	return 0;
}

int rwtest2(int nargs, char **args) {

	(void)nargs;
	(void)args;
    int i =0;

    kprintf_n("Starting rwt2...\n");

    test_status = TEST161_SUCCESS;

    for (i=0; i<CREATELOOPS; i++) {
        testrwlock = rwlock_create("testlock");
        if (testrwlock == NULL) {
            panic("rwt2: rwlock_create failed\n");
        }
        if (i != CREATELOOPS - 1) {
            rwlock_destroy(testrwlock);
        }
    }

    for (i=0; i<NTHREADS; i++) {
        kprintf_t(".");
        rwlock_acquire_read(testrwlock);
        rwlock_release_read(testrwlock);
        rwlock_acquire_write(testrwlock);
        rwlock_release_write(testrwlock);
    }

    success(test_status, SECRET, "rwt2");

    rwlock_destroy(testrwlock);
    testrwlock = NULL;

    return 0;
}

int rwtest3(int nargs, char **args) {

    (void)nargs;
    (void)args;
    int i =0;

    kprintf_n("Starting rwt3...\n");

    test_status = TEST161_SUCCESS;

    for (i=0; i<CREATELOOPS; i++) {
        testrwlock = rwlock_create("testlock");
        if (testrwlock == NULL) {
            panic("rwt3: rwlock_create failed\n");
        }
        if (i != CREATELOOPS - 1) {
            rwlock_destroy(testrwlock);
        }
    }

    for (i=0; i<NTHREADS; i++) {
        kprintf_t(".");
        rwlock_acquire_read(testrwlock);
        rwlock_acquire_write(testrwlock);
    }

    success(test_status, SECRET, "rwt3");

    rwlock_destroy(testrwlock);
    testrwlock = NULL;

    return 0;
}

int rwtest4(int nargs, char **args) {
    (void)nargs;
    (void)args;
    int i =0;

    kprintf_n("Starting rwt4...\n");

    test_status = TEST161_SUCCESS;

    for (i=0; i<CREATELOOPS; i++) {
        testrwlock = rwlock_create("testlock");
        if (testrwlock == NULL) {
            panic("rwt4: rwlock_create failed\n");
        }
        if (i != CREATELOOPS - 1) {
            rwlock_destroy(testrwlock);
        }
    }

    for (i=0; i<NTHREADS; i++) {
        kprintf_t(".");
        rwlock_release_read(testrwlock);
        rwlock_release_write(testrwlock);
    }

    success(test_status, SECRET, "rwt4");

    rwlock_destroy(testrwlock);
    testrwlock = NULL;

    return 0;
}

int rwtest5(int nargs, char **args) {
    (void)nargs;
    (void)args;
    int i =0;

    kprintf_n("Starting rwt5...\n");

    test_status = TEST161_SUCCESS;

    for (i=0; i<CREATELOOPS; i++) {
        testrwlock = rwlock_create("testlock");
        if (testrwlock == NULL) {
            panic("rwt5: rwlock_create failed\n");
        }
        if (i != CREATELOOPS - 1) {
            rwlock_destroy(testrwlock);
        }
    }

    for (i=0; i<NTHREADS; i++) {
        kprintf_t(".");
        rwlock_release_write(testrwlock);
    }

    success(test_status, SECRET, "rwt5");

    rwlock_destroy(testrwlock);
    testrwlock = NULL;

    return 0;
}