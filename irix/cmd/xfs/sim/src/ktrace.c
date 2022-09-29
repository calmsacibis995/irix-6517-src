
/*
 * The functions defined here are only included in DEBUG kernels.
 * Ktrace_alloc is needed as a stub for non-debug kernels, for now.
 * It should really be in stubs/.
 */

#define _KERNEL 1

#include <sys/param.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/ktrace.h>
#include <sys/debug.h>
#include <sys/atomic_ops.h>

#undef _KERNEL

#include <stdio.h>
#include "sim.h"

#ifndef DEBUG
/*ARGSUSED*/
ktrace_t *
ktrace_alloc(int nentries, int sleep)
{
	return NULL;
}
#else
/*
 * ktrace_alloc()
 *
 * Allocate a ktrace header and enough buffering for the given
 * number of entries.
 */
ktrace_t *
ktrace_alloc(int nentries, int sleep)
{
	ktrace_t	*ktp;
	ktrace_entry_t	*ktep;

	ktp = (ktrace_t*)kmem_alloc(sizeof(*ktp), sleep & KM_NOSLEEP);
	if (ktp == (ktrace_t*)NULL)
		return NULL;

	ktep = (ktrace_entry_t*)kmem_zalloc((nentries * sizeof(*ktep)),
					    sleep & KM_NOSLEEP);
	if (ktep)
		ktp->kt_entries = ktep;
	else {
		kmem_free(ktp, sizeof(*ktp));
		return NULL;
	}

	spinlock_init(&(ktp->kt_lock), "kt_lock");
	ktp->kt_nentries = nentries;
	ktp->kt_index = 0;
	ktp->kt_rollover = 0;

	return ktp;
}

/*
 * ktrace_free()
 *
 * Free up the ktrace header and buffer.  It is up to the caller
 * to ensure that no-one is referencing it.
 */
void
ktrace_free(ktrace_t *ktp)
{
	int	entries_size;

	if (ktp == (ktrace_t *)NULL)
		return;

	spinlock_destroy(&ktp->kt_lock);
	entries_size = (int)(ktp->kt_nentries * sizeof(ktrace_entry_t));
	kmem_free(ktp->kt_entries, entries_size);
	kmem_free(ktp, sizeof(ktrace_t));
}

/*
 * Enter the given values into the "next" entry in the trace buffer.
 * kt_index is always the index of the next entry to be filled.
 */
void
ktrace_enter(
	ktrace_t	*ktp,
	void		*val0,
	void		*val1,
	void		*val2,
	void		*val3,
	void		*val4,
	void		*val5,
	void		*val6,
	void		*val7,
	void		*val8,
	void		*val9,
	void		*val10,
	void		*val11,
	void		*val12,
	void		*val13,
	void		*val14,
	void		*val15)	     
{
	int		index;
	ktrace_entry_t	*ktep;

	ASSERT(ktp != NULL);

	/*
	 * Grab an entry by pushing the index up to the next one.
	 */
	index = atomicIncWithWrap(&ktp->kt_index, ktp->kt_nentries);
	if (!ktp->kt_rollover && index == ktp->kt_nentries - 1)
		ktp->kt_rollover = 1;
	ASSERT((index >= 0) && (index < ktp->kt_nentries));

	ktep = &(ktp->kt_entries[index]);
	ktep->val[0] = val0;
	ktep->val[1] = val1;
	ktep->val[2] = val2;
	ktep->val[3] = val3;
	ktep->val[4] = val4;
	ktep->val[5] = val5;
	ktep->val[6] = val6;
	ktep->val[7] = val7;
	ktep->val[8] = val8;
	ktep->val[9] = val9;
	ktep->val[10] = val10;
	ktep->val[11] = val11;
	ktep->val[12] = val12;
	ktep->val[13] = val13;
	ktep->val[14] = val14;
	ktep->val[15] = val15;

}

/*
 * routines from kern/io/idbg.c
 */

/*
 * Return the number of entries in the trace buffer.
 */
int
ktrace_nentries(
	ktrace_t	*ktp)
{
	if (ktp == NULL) {
		return 0;
	}

	return (ktp->kt_rollover ? ktp->kt_nentries : ktp->kt_index);
}

/*
 * ktrace_first()
 *
 * This is used to find the start of the trace buffer.
 * In conjunction with ktrace_next() it can be used to
 * iterate through the entire trace buffer.  This code does
 * not do any locking because it is assumed that it is called
 * from the debugger.
 *
 * The caller must pass in a pointer to a ktrace_snap
 * structure in which we will keep some state used to
 * iterate through the buffer.  This state must not touched
 * by any code outside of this module.
 */
ktrace_entry_t *
ktrace_first(ktrace_t	*ktp, ktrace_snap_t	*ktsp)
{
	ktrace_entry_t	*ktep;
	int		index;
	int		nentries;

	if (ktp->kt_rollover)
		index = ktp->kt_index;
	else
		index = 0;

	ktsp->ks_start = index;
	ktep = &(ktp->kt_entries[index]);
	
	nentries = ktrace_nentries(ktp);
	index++;
	if (index < nentries) {
		ktsp->ks_index = index;
	} else {
		ktsp->ks_index = 0;
		if (index > nentries)
			ktep = NULL;
	}
	return ktep;
}


/*
 * ktrace_next()
 *
 * This is used to iterate through the entries of the given
 * trace buffer.  The caller must pass in the ktrace_snap_t
 * structure initialized by ktrace_first().  The return value
 * will be either a pointer to the next ktrace_entry or NULL
 * if all of the entries have been traversed.
 */
ktrace_entry_t *
ktrace_next(
	ktrace_t	*ktp,
	ktrace_snap_t	*ktsp)
{
	int		index;
	ktrace_entry_t	*ktep;

	index = ktsp->ks_index;
	if (index == ktsp->ks_start) {
		ktep = NULL;
	} else {
		ktep = &ktp->kt_entries[index];
	}

	index++;
	if (index == ktrace_nentries(ktp)) {
		ktsp->ks_index = 0;
	} else {
		ktsp->ks_index = index;
	}

	return ktep;
}

/*
 * ktrace_skip()
 *
 * Skip the next "count" entries and return the entry after that.
 * Return NULL if this causes us to iterate past the beginning again.
 */
ktrace_entry_t *
ktrace_skip(
	ktrace_t	*ktp,
	int		count,	    
	ktrace_snap_t	*ktsp)
{
	int		index;
	int		new_index;
	ktrace_entry_t	*ktep;
	int		nentries = ktrace_nentries(ktp);

	index = ktsp->ks_index;
	new_index = index + count;
	while (new_index >= nentries) {
		new_index -= nentries;
	}
	if (index == ktsp->ks_start) {
		/*
		 * We've iterated around to the start, so we're done.
		 */
		ktep = NULL;
	} else if ((new_index < index) && (index < ktsp->ks_index)) {
		/*
		 * We've skipped past the start again, so we're done.
		 */
		ktep = NULL;
		ktsp->ks_index = ktsp->ks_start;
	} else {
		ktep = &(ktp->kt_entries[new_index]);
		new_index++;
		if (new_index == nentries) {
			ktsp->ks_index = 0;
		} else {
			ktsp->ks_index = new_index;
		}
	}
	return ktep;
}

#ifdef DEBUG_BUFTRACE
void
buf_trace_entry(
	ktrace_entry_t	*ktep)
{
	printf("%s ra %p cpu %d bp %p flags %x\n",
		(char *)(ktep->val[0]), ktep->val[9], ktep->val[1],
		ktep->val[2], ktep->val[3]);
	printf("offset 0x%x%x bcount %#x bufsize %#x blkno %#x\n",
		ktep->val[4], ktep->val[5], ktep->val[6],
		ktep->val[7], ktep->val[8]);
	printf("forw %p back %p vp %p\n",
		ktep->val[10], ktep->val[11], ktep->val[12]);
	printf("dforw %p dback %p lock %d\n",
		ktep->val[13], ktep->val[14], ktep->val[15]);
}

void
idbg_buftrace(buf_t *bp)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;

	if (bp->b_trace == NULL) {
		printf("The buffer trace buffer is not initialized\n");
		return;
	}

	printf("buftrace bp %p\n", bp);
	ktep = ktrace_first(bp->b_trace, &kts);
	while (ktep != NULL) {
		printf("\n");
		buf_trace_entry(ktep);
		ktep = ktrace_next(bp->b_trace, &kts);
	}
}
#endif /* DEBUG_BUFTRACE */
#endif /* DEBUG */
