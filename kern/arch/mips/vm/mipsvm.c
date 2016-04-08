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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 *
 * NOTE: it's been found over the years that students often begin on
 * the VM assignment by copying dumbvm.c and trying to improve it.
 * This is not recommended. dumbvm is (more or less intentionally) not
 * a good design reference. The first recommendation would be: do not
 * look at dumbvm at all. The second recommendation would be: if you
 * do, be sure to review it from the perspective of comparing it to
 * what a VM system is supposed to do, and understanding what corners
 * it's cutting (there are many) and why, and more importantly, how.
 */

void
vm_bootstrap(void)
{
	/* Do nothing. */
}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */
static
void
dumbvm_can_sleep(void)
{

}

static
paddr_t
getppages(unsigned long npages)
{
	unsigned long count = 0, page_counter = ram_getsize() / PAGE_SIZE, i;

	for(i = coremap_addr/PAGE_SIZE; i < page_counter && count != npages; i++) {
		if(coremap[i].state == FREE) {
			count++;
		} else {
			count = 0;
		}
	}

	if (count < npages) {
		return 0;
	} else {
		i--;
	}

	while(count > 0) {
		coremap[i].state = FIXED;

		if (count == 1) {
			coremap[i].chunk_size = npages;
			break;
		}
		i--;
		count--;
	}

	return i * PAGE_SIZE;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	(void) npages;
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void
free_kpages(vaddr_t addr)
{
	int index = (addr - MIPS_KSEG0) / PAGE_SIZE;
	int pages = coremap[index].chunk_size;

	coremap[index].chunk_size = 0;

	for (int i = 0; i < pages; i++) {
		coremap[index + i].state = FREE;
	}

}

unsigned
int
coremap_used_bytes() {

	int count = 0;
	for (unsigned int i = 0 ; i < ram_getsize() / PAGE_SIZE; i++) {
		if (coremap[i].state == FIXED) {
			count++;
		}
	}

	return count * PAGE_SIZE;
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	(void) faulttype, (void) faultaddress;
	return 0;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	return as;
}

void
as_destroy(struct addrspace *as)
{
	dumbvm_can_sleep();
	kfree(as);
}

void
as_activate(void)
{

}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
				 int readable, int writeable, int executable)
{
	(void) as, (void) vaddr, (void) sz, (void) readable, (void) writeable, (void) executable;
	return 0;
}

/*
static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	(void) paddr, (void) npages;
}
*/

int
as_prepare_load(struct addrspace *as)
{
	(void) as;

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	dumbvm_can_sleep();

	(void)as;

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	(void) as, (void) stackptr;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	(void) old, (void) ret;
	return 0;
}
