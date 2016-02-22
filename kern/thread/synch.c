/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	lock->lk_wchan = wchan_create(lock->lk_name);
	if (lock->lk_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	lock->lk_thread = NULL;
	spinlock_init(&lock->lk_lock);

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(!lock_do_i_hold(lock));

	spinlock_cleanup(&lock->lk_lock);
	wchan_destroy(lock->lk_wchan);

	kfree(lock->lk_name);
	kfree(lock);
}

/*
 * Acquire lock by making atomic operation
 * If unable to acquire lock sleep in a wait channel
 * Repeat the process until a lock can be acquired
 * Once successful in acquiring the lock, release the spinlock
 */

void
lock_acquire(struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(!lock_do_i_hold(lock));

	spinlock_acquire(&lock->lk_lock);

	while(true) {
		if (lock->lk_thread == NULL) {
			lock->lk_thread = curthread;
			spinlock_release(&lock->lk_lock);
			break;
		}

		wchan_sleep(lock->lk_wchan, &lock->lk_lock);
	}
}

void
lock_release(struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&lock->lk_lock);

	wchan_wakeone(lock->lk_wchan, &lock->lk_lock);

	lock->lk_thread = NULL;
	spinlock_release(&lock->lk_lock);

}

bool
lock_do_i_hold(struct lock *lock)
{
	return lock->lk_thread == curthread;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);

	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}

	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->cv_lock);
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

	wchan_destroy(cv->cv_wchan);
	spinlock_cleanup(&cv->cv_lock);

	kfree(cv->cv_name);
	kfree(cv);
}

 /*
 * Do not sleep with the lock acquired
 * Make sure the lock release and sleep is atomic operation
 * Re-acquire lock once woken up
 */

void
cv_wait(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_lock);

	lock_release(lock);
	wchan_sleep(cv->cv_wchan, &cv->cv_lock);

	spinlock_release(&cv->cv_lock);
	lock_acquire(lock);
}

/*
 * Wake up the one lucky thread
 */

void
cv_signal(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_lock);

	wchan_wakeone(cv->cv_wchan, &cv->cv_lock);

	spinlock_release(&cv->cv_lock);
}

/*
 * Wake up all the threads and let them fight for lock contention
 */

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_lock);

	wchan_wakeall(cv->cv_wchan, &cv->cv_lock);

	spinlock_release(&cv->cv_lock);
}

struct
rwlock *rwlock_create(const char *name) {

	struct rwlock *rw_lock;

	rw_lock = kmalloc(sizeof(*rw_lock));
	if (rw_lock == NULL) {
		return NULL;
	}

	rw_lock->rwlock_name = kstrdup(name);

	if (rw_lock->rwlock_name==NULL) {
		kfree(rw_lock);
		return NULL;
	}

	rw_lock->rwlock_wchan = wchan_create(rw_lock->rwlock_name);
	if (rw_lock->rwlock_wchan == NULL) {
		kfree(rw_lock->rwlock_name);
		kfree(rw_lock);
		return NULL;
	}

	spinlock_init(&rw_lock->rwlock_lock);
	rw_lock->rwlock_is_writing = false;
	rw_lock->rwlock_reader_count = 0;
	rw_lock->rwlock_writer_count = 0;

	return rw_lock;
}

void
rwlock_destroy(struct rwlock *rw_lock) {

	KASSERT(rw_lock != NULL);
	KASSERT(!rw_lock->rwlock_is_writing);
	KASSERT(rw_lock->rwlock_reader_count == 0);

	wchan_destroy(rw_lock->rwlock_wchan);
	spinlock_cleanup(&rw_lock->rwlock_lock);

	kfree(rw_lock->rwlock_name);
	kfree(rw_lock);
}

/*
 * Reader will wait in a channel if a writer is doing its job. Once it wakes up the count is increased.
 */

void
rwlock_acquire_read(struct rwlock *rw_lock) {

	KASSERT(rw_lock != NULL);

	spinlock_acquire(&rw_lock->rwlock_lock);

	while (rw_lock->rwlock_is_writing || (rw_lock->rwlock_reader_count > rw_lock->rwlock_writer_count
										  && rw_lock->rwlock_writer_count > 0)) {
		wchan_sleep(rw_lock->rwlock_wchan, &rw_lock->rwlock_lock);
	}

	KASSERT(!rw_lock->rwlock_is_writing);

	rw_lock->rwlock_reader_count++;
	spinlock_release(&rw_lock->rwlock_lock);

	return;
}

/*
 * The reader decreases the reader count and and wakes up all threads.
 */

void
rwlock_release_read(struct rwlock *rw_lock) {

	KASSERT(rw_lock != NULL);
	KASSERT(rw_lock->rwlock_reader_count > 0);

	spinlock_acquire(&rw_lock->rwlock_lock);

	rw_lock->rwlock_reader_count--;

	wchan_wakeall(rw_lock->rwlock_wchan, &rw_lock->rwlock_lock);

	spinlock_release(&rw_lock->rwlock_lock);

	return;
}

/*
 * The writer waits in a channel until it gets writer lock. Once it gets the lock the boolean is set to true.
 */

void
rwlock_acquire_write(struct rwlock *rw_lock) {

	KASSERT(rw_lock != NULL);

	spinlock_acquire(&rw_lock->rwlock_lock);

	rw_lock->rwlock_writer_count++;

	while (rw_lock->rwlock_reader_count > 0 || rw_lock->rwlock_is_writing) {
		wchan_sleep(rw_lock->rwlock_wchan, &rw_lock->rwlock_lock);
	}

	rw_lock->rwlock_is_writing = true;

	spinlock_release(&rw_lock->rwlock_lock);

	return;
}

/*
 * Release the lock by setting the boolean to false and wake up all threads.
 */

void
rwlock_release_write(struct rwlock *rw_lock) {

	KASSERT(rw_lock != NULL);
	KASSERT(rw_lock->rwlock_is_writing);

	spinlock_acquire(&rw_lock->rwlock_lock);

	rw_lock->rwlock_is_writing = false;

	rw_lock->rwlock_writer_count--;

	wchan_wakeall(rw_lock->rwlock_wchan, &rw_lock->rwlock_lock);

	spinlock_release(&rw_lock->rwlock_lock);

	return;

}