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
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>


struct cv *cv_whale_male, *cv_whale_female, *cv_whale_matchmaker;
int male_count, female_count, matchmaker_count;
struct lock *whale_lock;

/*
 * Called by the driver during initialization.
 */

void whalemating_init() {
	cv_whale_male = cv_create("whale_male");
	KASSERT(cv_whale_male != NULL);

	cv_whale_female = cv_create("whale_female");
	KASSERT(cv_whale_female != NULL);

	cv_whale_matchmaker = cv_create("whale_matchmaker");
	KASSERT(cv_whale_matchmaker != NULL);

	whale_lock = lock_create("whale_lock");
	KASSERT(whale_lock != NULL);

	male_count = female_count = matchmaker_count = 0;

	return;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	cv_destroy(cv_whale_male);
	cv_destroy(cv_whale_female);
	cv_destroy(cv_whale_matchmaker);
	lock_destroy(whale_lock);

	return;
}

/*
 * The male should start mating if at least a female and a matchmaker whales are present.
 * If the male cannot mate then wait in a channel and wait for a female whale or a matchmaker
 * whale to wake up the male whale.
 * Reduce the counters of female and matchmaker whales to disallow other whales from pairing
 * up with these whales.
 */
void
male(uint32_t index) {
	male_start(index);

	lock_acquire(whale_lock);
	male_count++;

	if (female_count > 0 && matchmaker_count > 0) {
		female_count--;
		cv_signal(cv_whale_female, whale_lock);

		matchmaker_count--;
		cv_signal(cv_whale_matchmaker, whale_lock);

		male_count--;
	} else {
		cv_wait(cv_whale_male, whale_lock);
	}

	male_end(index);
	lock_release(whale_lock);

	return;
}

void
female(uint32_t index) {
	female_start(index);

	lock_acquire(whale_lock);
	female_count++;

	if (male_count > 0 && matchmaker_count > 0) {
		male_count--;
		cv_signal(cv_whale_male, whale_lock);

		matchmaker_count--;
		cv_signal(cv_whale_matchmaker, whale_lock);

		female_count--;
	} else {
		cv_wait(cv_whale_female, whale_lock);
	}

	female_end(index);
	lock_release(whale_lock);

	return;
}

void
matchmaker(uint32_t index) {
	matchmaker_start(index);

	lock_acquire(whale_lock);
	matchmaker_count++;

	if (female_count > 0 && male_count > 0) {
		male_count--;
		cv_signal(cv_whale_male, whale_lock);

		female_count--;
		cv_signal(cv_whale_female, whale_lock);

		matchmaker_count--;
	} else {
		cv_wait(cv_whale_matchmaker, whale_lock);
	}

	matchmaker_end(index);
	lock_release(whale_lock);

	return;
}
