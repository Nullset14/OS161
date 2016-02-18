/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization
 */

struct semaphore *intersection;

/*
 * Locks for each of the quadrants
 */

struct lock *lock_0;
struct lock *lock_1;
struct lock *lock_2;
struct lock *lock_3;

void
stoplight_init() {

	/*
	 * In an intersection there have to be 3 cars at any point in time to avoid deadlock
	 */

	intersection = sem_create("intersection", 3);
	KASSERT(intersection != NULL);

	lock_0 = lock_create("lock_0");
	KASSERT(lock_0 != NULL);

	lock_1 = lock_create("lock_1");
	KASSERT(lock_1 != NULL);

	lock_2 = lock_create("lock_2");
	KASSERT(lock_2 != NULL);

	lock_3 = lock_create("lock_3");
	KASSERT(lock_3 != NULL);

	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	sem_destroy(intersection);
	lock_destroy(lock_0);
	lock_destroy(lock_1);
	lock_destroy(lock_2);
	lock_destroy(lock_3);
	return;
}

/*
 * Helper functions to guide the cars going in a specific direction with requisite locks
 */

void right_and_leave(struct lock *lock_11, uint32_t direction, uint32_t index);
void straight_and_leave(struct lock *lock_11, struct lock *lock_21, uint32_t direction, uint32_t index);
void left_and_leave(struct lock *lock_11, struct lock *lock_21, struct lock *lock_31, uint32_t direction, uint32_t index);

/*
 * Simplest of all the turns. Acquire the lock of the quadrant the car is present in, move forward and then leave
 */

void
right_and_leave(struct lock *lock_11, uint32_t direction, uint32_t index) {

	P(intersection);

	lock_acquire(lock_11);
	inQuadrant(direction, index);

	leaveIntersection(index);
	lock_release(lock_11);

	V(intersection);

	return;
}

/*
 *  This move requires acquiring locks sequentially. Move a step forward into the quadrant once each lock is
 *  acquired. Leave the intersection once in final quadrant and release the lock.
 */

void
straight_and_leave(struct lock *lock_11, struct lock *lock_21, uint32_t direction, uint32_t index) {

	P(intersection);

	lock_acquire(lock_11);
	inQuadrant(direction, index);

	lock_acquire(lock_21);
	inQuadrant((direction + 3) % 4, index);

	lock_release(lock_11);

	leaveIntersection(index);
	lock_release(lock_21);

	V(intersection);

	return;
}

/*
 *  This move requires acquiring locks sequentially. Move a step forward into the quadrant once each lock is
 *  acquired. Leave the intersection once in final quadrant and release the lock.
 */

void
left_and_leave(struct lock *lock_11, struct lock *lock_21, struct lock *lock_31, uint32_t direction, uint32_t index) {

	P(intersection);

	lock_acquire(lock_11);
	inQuadrant(direction, index);

	lock_acquire(lock_21);
	inQuadrant((direction + 3) % 4, index);
	lock_release(lock_11);

	lock_acquire(lock_31);
	inQuadrant((direction + 2) % 4, index);
	lock_release(lock_21);

	leaveIntersection(index);
	lock_release(lock_31);

	V(intersection);

	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
	switch(direction) {
		case 0 :
			right_and_leave(lock_0, direction, index);
			break;
		case 1 :
			right_and_leave(lock_1, direction, index);
			break;
		case 2 :
			right_and_leave(lock_2, direction, index);
			break;
		case 3 :
			right_and_leave(lock_3, direction, index);
			break;
	}
	return;
}

void
gostraight(uint32_t direction, uint32_t index)
{
	switch(direction) {
		case 0 :
			straight_and_leave(lock_0, lock_3, direction, index);
			break;
		case 1 :
			straight_and_leave(lock_1, lock_0, direction, index);
			break;
		case 2 :
			straight_and_leave(lock_2, lock_1, direction, index);
			break;
		case 3 :
			straight_and_leave(lock_3, lock_2, direction, index);
			break;
	}

	return;
}

void
turnleft(uint32_t direction, uint32_t index)
{
	switch(direction) {
		case 0 :
			left_and_leave(lock_0, lock_3, lock_2, direction, index);
			break;
		case 1 :
			left_and_leave(lock_1, lock_0, lock_3, direction, index);
			break;
		case 2 :
			left_and_leave(lock_2, lock_1, lock_0, direction, index);
			break;
		case 3 :
			left_and_leave(lock_3, lock_2, lock_1, direction, index);
			break;
	}

	return;
}