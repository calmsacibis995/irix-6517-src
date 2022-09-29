/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/kmem.h>
#include <sys/immu.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/schedctl.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/par.h>
#include <sys/atomic_ops.h>
#include <sys/rtmon.h>
#include <sys/idbgentry.h>

#include "tstamp.h"

/*
 * Macros to simplify access to per cpu tstamp object
 */

#define	TEST_CPU_INRANGE(cpu)	(0 <= cpu && cpu < numcpus)
#define	TEST_CPU_OK(cpu) \
	(TEST_CPU_INRANGE(cpu) && pdaindr[cpu].CpuId != -1)
#define	TSTAMP_OBJP(cpu) \
	((tstamp_obj_t*) pdaindr[cpu].pda->p_tstamp_objp)
#define TEST_TSTAMP_OBJECT(cpu) \
	(TSTAMP_OBJP(cpu) != (tstamp_obj_t*) 0)
#define TSTAMP_EOBMODE(cpu)		(pdaindr[cpu].pda->p_tstamp_eobmode)
#define TSTAMP_ENTRIES(pda)		((tstamp_event_entry_t *)pda->p_tstamp_entries)
#define TSTAMP_LAST(pda)		((tstamp_event_entry_t *)pda->p_tstamp_last)

#if ((! defined(_TSTAMP_PRESERVE_BUFFERS)) && MP)
#define _TSTAMP_PRESERVE_BUFFERS 1
#endif /* ((! defined(_TSTAMP_PRESERVE_BUFFERS)) && MP) */

#ifdef _TSTAMP_PRESERVE_BUFFERS
#define TSTAMP_SPL_DECL() 
#define TSTAMP_SPL()  
#define TSTAMP_SPLX() 
#else 
#define TSTAMP_SPL_DECL()  int _tstamp_sval;
#define TSTAMP_SPL() _tstamp_sval = spl7()
#define TSTAMP_SPLX() (void) splx(_tstamp_sval)
#endif

/* systune variables */
extern long long utrace_mask;
extern int utrace_bufsize;

#define TRUE 1
#define FALSE 0
    
tstamp_obj_t **tstamp_indr = NULL;

int
tstamp_init(int cpu)
{
	tstamp_obj_t **ti;

	if (tstamp_indr != NULL) 
		return(0);
	ti = (tstamp_obj_t**) kmem_zalloc_node_hint((sizeof(tstamp_obj_t *) * numcpus),
						    KM_NOSLEEP, 
						    pdaindr[cpu].pda->p_nodeid);
	if (ti == NULL)
		return(1);
	if (compare_and_swap_ptr((void **) &tstamp_indr,
				 NULL,
				 (void *) ti))
		return(0);
	kmem_free(ti, sizeof(tstamp_obj_t *) * numcpus);
	return(0);
}

tstamp_obj_t*
tstamp_construct(int nentries, int cpu)
{
	tstamp_obj_t* ts;
	__psunsigned_t p;
	uint	asize;

	if (nentries < 0)
		return ((tstamp_obj_t*) 0);

	if (tstamp_indr == NULL &&
	    tstamp_init(cpu))
		return((tstamp_obj_t *) 0);

	/*
	 * Allocate memory for the object itself.  Get it on the proper node
	 * if possible.
	 */
	ts = tstamp_indr[cpu];
	if (ts == NULL) {
		ts = (tstamp_obj_t*) kmem_zalloc_node_hint(sizeof (tstamp_obj_t),
							   KM_NOSLEEP|VM_CACHEALIGN,
							   pdaindr[cpu].pda->p_nodeid);
		if (ts == ((tstamp_obj_t*) 0))
			return ((tstamp_obj_t*) 0);
		initnsema(&ts->wmsema, 0, "tstamp");
		if (! compare_and_swap_ptr((void **) (tstamp_indr + cpu),
					   NULL,
					   (void *) ts)) {
			freesema(&ts->wmsema);
			kmem_free((void *) ts, sizeof(tstamp_obj_t));
			ts = tstamp_indr[cpu];
		}
	}
	if (ts->nentries == 0) {
		/* 
		 * We need one extra page to guarantee page alignment
		 * for mapping the event entry array into a
		 * user's address space.  An additional page is needed
		 * to map the state by all cpus.
		 */
		asize = (nentries*sizeof (tstamp_event_entry_t)) + 2*NBPP;
		p = (__psunsigned_t) kmem_zalloc_node_hint(asize,
							   KM_NOSLEEP|VM_VM, 
							   pdaindr[cpu].pda->p_nodeid);
		if (p == 0)
			return ((tstamp_obj_t*) 0);

		ts->allocated_size = asize;
		ts->allocated_base = p;
		ts->shared_state = (tstamp_shared_state_t*)((p + NBPP) & (~POFFMASK));
		ts->water_mark = nentries / 2;
		ts->tracemask = 0;
		ts->nentries = nentries;

	}
	tstamp_reset(ts, TRUE);

	return(ts);
}

void
tstamp_destruct(tstamp_obj_t* ts){
#ifndef _TSTAMP_PRESERVE_BUFFERS
	__psunsigned_t p;
	uint	asize;
#endif /* _TSTAMP_PRESERVE_BUFFERS */

	ASSERT(ts != (tstamp_obj_t*) 0);
	while (vsema(&ts->wmsema)) {
		/* keep releasing while somebody is waiting */
		;
	}

#ifndef _TSTAMP_PRESERVE_BUFFERS
	if (ts->nentries) {
		p = ts->allocated_base;
		asize = ts->allocated_size;
		ts->allocated_size = 0;
		ts->allocated_base = 0;
		ts->shared_state = NULL;
		ts->water_mark = 0;
		ts->tracemask = 0;
		ts->nentries = 0;

		kmem_free((void *) p, asize);
	}
#endif /* _TSTAMP_PRESERVE_BUFFERS */
}

/*
 * The install and uninstall operations 
 * MUST be done with the corresponding
 * tstamp pda lock LOCKED.
 */

void
tstamp_install(tstamp_obj_t* ts, int cpu)
{
	ASSERT(ts != (tstamp_obj_t*)0);
	ASSERT(TEST_CPU_INRANGE(cpu));

	pdaindr[cpu].pda->p_tstamp_objp = (void *)ts;

	pdaindr[cpu].pda->p_tstamp_eobmode = RT_TSTAMP_EOB_STOP;
	pdaindr[cpu].pda->p_tstamp_entries = (tstamp_event_entry_t*)
	    ((__psunsigned_t)(ts->shared_state) + TSTAMP_SHARED_STATE_LEN);
	pdaindr[cpu].pda->p_tstamp_last = 
		(tstamp_event_entry_t*)pdaindr[cpu].pda->p_tstamp_entries + 
		ts->nentries - 1;
	pdaindr[cpu].pda->p_tstamp_ptr = NULL; /* set in tstamp_set_eob_mode() */
}

tstamp_obj_t*
tstamp_uninstall(int cpu)
{
	tstamp_obj_t* ts;
	ASSERT(TEST_CPU_INRANGE(cpu));
	ASSERT(TSTAMP_OBJP(cpu) != 0);
	ts = TSTAMP_OBJP(cpu);
	pdaindr[cpu].pda->p_tstamp_objp = (void *) 0;

#if !defined(_TSTAMP_PRESERVE_BUFFERS)
	/*
	 * Because we have no lock around the timestamp buffer
	 * wait long enough
	 * to know that if a timestamp was being recorded that
	 * it is now finished.
	 *
	 * Note that this only works because log_tstamp_event now
	 * runs as spl7(), so it cannot be switched to a different processor
	 * while executing.
	 */
	delay(HZ/2);

	pdaindr[cpu].pda->p_tstamp_entries = NULL;
#endif
	pdaindr[cpu].pda->p_tstamp_eobmode = 0;
#if !defined(_TSTAMP_PRESERVE_BUFFERS)
	pdaindr[cpu].pda->p_tstamp_last = NULL;
	pdaindr[cpu].pda->p_tstamp_ptr = NULL;
#endif

	return (ts);
}


paddr_t
tstamp_get_buffer_paddr(tstamp_obj_t* ts)
{
	ASSERT(ts != (tstamp_obj_t*) 0);
	return ((paddr_t)ts->shared_state);
}

paddr_t
tstamp_get_buffer_paddr_from_cpu(int cpu)
{
	ASSERT(TSTAMP_OBJP(cpu) != 0);
	return ((paddr_t) TSTAMP_OBJP(cpu)->shared_state);
}


/*
 * Enter an event that is known to fit in a single
 * tstamp event record.
 */

void
log_tstamp_event(event_t evt, 
		 __int64_t qual0, 
		 __int64_t qual1, 
		 __int64_t qual2, 
		 __int64_t qual3)
{
	tstamp_obj_t* ts;
	tstamp_event_entry_t *event, *next_event;
	__uint64_t time;
	uint index, next_index;
	pda_t *pda;
	TSTAMP_SPL_DECL()

	TSTAMP_SPL();	

	/* 
	 * Get pda ptr in a local.  If we get an interrupt, we could end up
	 * on a different processor, with a different mapping of "private".
	 */
	pda = pdaindr[cpuid()].pda;

	/* Fast path if the daemon is not active */
	if (pda->p_tstamp_eobmode != RT_TSTAMP_EOB_STOP) {

		time = absolute_rtc_current_time();

		/* Do tracing more efficiently, ala UNICOS' UTRACE()s.
		   - Since the trace mask was read just before calling this
			 routine, keeping the buffer pointers next to it in the
			 pda avoids extra memory traffic.
		   - There's no need to maintain the shared state, since no
			 one is reading the buffer. 
		   - There are no function calls along this path (if ASSERT
			 compiles out and absolute_rtc_current_time() expands
			 inline), so we save a pile of stack setup overhead. */

		/* We can get an interrupt between reading and writing
		   pda->p_tstamp_ptr, which causes us to overwrite the
		   interrupt's traces when we resume.  However, atomic updates
		   (see below) slow the fast path by about 33% on an O2000.
		   We'd rather have the speed, since the problem is rare and
		   absolute correctness isn't required for this sort of
		   debugging info.  A similar issue exists in UNICOS. */

		event = pda->p_tstamp_ptr;
		/* The compiler generates better code for pointer
		   arithmetic than for array indexing when
		   sizeof(utrace_entry_t) is not a power of 2 */
		next_event = (event < TSTAMP_LAST(pda) ? (event + 1) :
					  TSTAMP_ENTRIES(pda));
		pda->p_tstamp_ptr = next_event;

#if 0
		/* This loop is similar to atomicIncWithWrap(), but inline and
		   for pointers.  It could be much more efficiently written in
		   assembly language. */
		do {
			event = pda->p_tstamp_ptr;
			next_event = (event < TSTAMP_LAST(pda)
					? (event + 1)
					: TSTAMP_ENTRIES(pda));
		} while (!__compare_and_swap((__psint_t *)(&pda->p_tstamp_ptr),
					     (__psint_t)event,
					     (__psint_t)next_event));
#endif

		ASSERT(event->jumbocnt == 0); /* jumbocnt unused in wrap mode */
		event->evt = evt;
		event->qual[0] = qual0;
		event->qual[1] = qual1;
		event->qual[2] = qual2;
		event->qual[3] = qual3;
		/* This store should be done as long as possible after the
		   (slow, uncached) rtc read.  Is there a good way to keep the
		   compiler from reordering it earlier?  Does it matter? */
		event->tstamp = time;
		TSTAMP_SPLX();
		return;
	}

	ts = pda->p_tstamp_objp;
	if (ts == NULL) {
		TSTAMP_SPLX();
		return;
	}

	/*
	 * If the buffer is known to be full,
	 * discard the event.
	 */
	if (ts->shared_state->lost_counter) {
		atomicAddInt(&ts->shared_state->lost_counter, 1);
		TSTAMP_SPLX();
		return;
	}
	/*
	 * Grab the next entry in the queue
	 */
	index = atomicIncWithWrap(&ts->shared_state->curr_index, ts->nentries);
	time = absolute_rtc_current_time();	/* AFTER getting index */
	event = &((TSTAMP_ENTRIES(pda))[index]);
	/*
	 * The event reading thread marks events free by
	 * zeroing the evt field.  If the event is still
	 * unread then drop the event we're to insert
	 */
	if (event->evt != 0) {
		atomicAddInt(&ts->shared_state->lost_counter, 1);
		TSTAMP_SPLX();
		return;
	}

	/* 
	 * NB: it's not worth deciding which quals to
	 * record based on the event; just write 'em all.
	 */
	event->qual[0] = qual0;
	event->qual[1] = qual1;
	event->qual[2] = qual2;
	event->qual[3] = qual3;
	/*
	 * Make sure that tstamps happen in order in the kernel
	 * queue.  They might get out of order if we are in the 
	 * process of logging something and an interrupt comes in
	 * before we have the index, but after we get the time.
	 */
	next_index = (index+1 >= ts->nentries ? 0 : index+1);
	if (next_index == ts->shared_state->curr_index) { /* common case */
			event->tstamp = time;
	}
	else {
		next_event = (event < TSTAMP_LAST(pda) ? (event + 1) :
							 TSTAMP_ENTRIES(pda));
		if (next_event->evt == 0 ||
			time < next_event->tstamp) {
			event->tstamp = time;
		} else {
			/*
			 * The 1 is just a value to make the current time
			 * less than the next timestamp.
			 */
			event->tstamp = next_event->tstamp - 1;
		}
	}

	event->jumbocnt = 0;
	/*
	 * Mark the event type last because that is what the
	 * user-level process checks for to identify when a
	 * tstamp is complete.
	 */
	event->evt = evt;
	/*
	 * Increment the count of tstamps and check the fill
	 * state.  When the queue level reaches the water mark
	 * wakeup the monitoring process.
	 */
	atomicAddInt(&ts->shared_state->tstamp_counter, 1);
	/*
	 * Cannot wakeup the event collection thread if we're
	 * called from putrunq.  To check for this we check that
	 * we are not logging TSTAMP_EV_EODISP (the event that
	 * is logged only from putrunq).
	 *
	 * The putrunq path must be avoided because if the waiting
	 * thread is running w/ realtime priority the vsema call
	 * will result in another putrunq call during which we
	 * try to acquire the global rt q lock,  but a parallel
	 * rt_thread_select call may already have grabbed that
	 * lock and be trying to lock the thread which already
	 * is on the queue.  Basically any path that can result
	 * in recursive putrunq calls can cause this inversion of
	 * the lock hierarchy.
	 *
	 * Also, we can not call vsema() if the current spl level is
	 * above splhi.  vsema() assumes that splhi() blocks all
	 * callers, however some interrupts are not masked by splhi.
	 * This can cause vsema() to be reentered unexpectedly.  That
	 * is,  intr() calls log_tstamp_event() and therefore vsema()
	 * at, say, spltty().  vsema() sets splhi(), but then a
	 * count/compare interrupt occurs, and it reenters vsema().
	 * Bug 545826.
	 *
	 * Note that 2 threads may be racing to do the vsema. The
	 * vsema should be done ONLY if the atomic decrement of 
	 * nsleepers is successful. Otherwise, nsleepers can be driven
	 * negative; this causes us to repeatly vsema & eventually overflow
	 * the sema count.
	 * Bug 553748.
	 */
	if (ts->shared_state->tstamp_counter >= ts->water_mark &&
	    ts->nsleepers &&
	    evt != TSTAMP_EV_EODISP) {
		TSTAMP_SPLX();
		if (!issplprof(getsr())) {
			int s = splhi();

			ts = pda->p_tstamp_objp;
			if (ts != NULL &&
			    ts->shared_state != NULL &&
			    ts->shared_state->tstamp_counter >= ts->water_mark &&
			    ts->nsleepers) {
				if (compare_and_dec_int_gt(&ts->nsleepers, 0))
					vsema(&ts->wmsema);
			}
			splx(s);
		}
	} else {
		TSTAMP_SPLX();
	}
}

/*
 * Enter an event that requires more space than a
 * single tstamp event record.  The space needed to
 * hold the additional information is allocated as
 * one or more tstamp event records that are logically
 * contiguous in the queue.  In stop mode, data following 
 * the first record is contiguous and the jumbocnt field
 * of the first record is used to identify the number of
 * additional records that comprise the "jumbo event".
 * In wrap mode, the jumbocnt field is always zero and
 * the additional data is stored in one or more event
 * records following the initial record, each of which has
 * the special event type, JUMBO_EVENT.
 */
void
log_tstamp_vevent(event_t evt, const void* data, uint xlen)
{
	tstamp_obj_t* ts;
	__scunsigned_t len = xlen; /* scaling ints are better for ptr math */
	__uint64_t time;
	uint index, n, nentries, next_index;
	tstamp_event_entry_t *event, *nevent;
	pda_t *pda;
	TSTAMP_SPL_DECL()

	TSTAMP_SPL();

	/* 
	 * Get pda ptr in a local.  If we get an interrupt, we could end up
	 * on a different processor, with a different mapping of "private".
	 */
	pda = pdaindr[cpuid()].pda;

	if (pda->p_tstamp_eobmode != RT_TSTAMP_EOB_STOP) {
		__uint64_t *p;

		/*
		 * In wrap mode, we can't store the data contiguously due to a
		 * Catch-22 between updating the pointer (possibly pointing it
		 * into the middle of a jumbo event) and breaking up the jumbo
		 * events.  Each has to be done first to do the other safely.
		 * So instead, we store up to five 64-bit values in each entry
		 * after the first, using the tstamp and qual[] fields.
		 */

		time = absolute_rtc_current_time();
		nentries = 1 +
			howmany(len - sizeof (TSTAMP_ENTRIES(pda)[0].qual),
					sizeof(TSTAMP_ENTRIES(pda)[0].qual) + 
					sizeof(TSTAMP_ENTRIES(pda)[0].tstamp));

		/* Atomic ops. are more expensive than they're worth.  See
		   the comments in log_tstamp_event. */
		event = (tstamp_event_entry_t *)pda->p_tstamp_ptr;
		nevent = event + nentries;
		if (nevent > TSTAMP_LAST(pda))
			nevent = (tstamp_event_entry_t*)
				((char *)nevent - 
				 ((char *)TSTAMP_LAST(pda) -
					 (char *)TSTAMP_ENTRIES(pda) + 
					  sizeof(tstamp_event_entry_t)));
		pda->p_tstamp_ptr = nevent;

#if 0
		/* This loop is like atomicAddWithWrap(), but works on ptrs */
		do {
			event = (tstamp_event_entry_t *)pda->p_tstamp_ptr;
			nevent = event + nentries;
			if (nevent > TSTAMP_LAST(pda))
				nevent = (tstamp_event_entry_t*)
					((char *)nevent -
					 ((char *)TSTAMP_LAST(pda) -
					  (char *)TSTAMP_ENTRIES(pda) + 
					  sizeof(tstamp_event_entry_t)));
		} while (!__compare_and_swap((__psint_t *)(&pda->p_tstamp_ptr),
					     (__psint_t)event,
					     (__psint_t)nevent));
#endif

		ASSERT(event->jumbocnt == 0); /* jumbocnt unused in wrap mode */

		p = (__uint64_t *)data;	/* assume data is 64-bit aligned */
		event->evt = evt;
		event->qual[0] = p[0];
		event->qual[1] = p[1];
		event->qual[2] = p[2];
		event->qual[3] = p[3];
		/* This store should be done as long as possible after the
		   (slow, uncached) rtc read.  Is there a good way to keep the
		   compiler from reordering it earlier?  Do we care? */
		event->tstamp = time;

		/* This loop could no doubt be highly optimized, ala bcopy */
		p += 4; len -= 4 * sizeof(__uint64_t);
		while(len >= 5 * sizeof(__uint64_t)) {
			event = (event < TSTAMP_LAST(pda) ? (event + 1) :
					 TSTAMP_ENTRIES(pda));
			ASSERT(event->jumbocnt == 0);

			event->evt     = JUMBO_EVENT;
			event->tstamp  = p[0];
			event->qual[0] = p[1];
			event->qual[1] = p[2];
			event->qual[2] = p[3];
			event->qual[3] = p[4];

			p += 5; len -= 5 * sizeof(__uint64_t);
		}

		if (len) {
			/* Length may not be aligned, use bcopy on last one */
			event = (event < TSTAMP_LAST(pda) ? (event + 1) :
					 TSTAMP_ENTRIES(pda));
			ASSERT(event->jumbocnt == 0);
			event->evt = JUMBO_EVENT;
			bcopy(p, &event->tstamp, len);
		}

		TSTAMP_SPLX();
		return;
	}

	/*
	 * If the buffer is known to be full,
	 * discard the event.
	 */
	ts = pda->p_tstamp_objp;
	if (ts == NULL) {
		TSTAMP_SPLX();
		return;
	}

	if (ts->shared_state->lost_counter) {
		atomicAddInt(&ts->shared_state->lost_counter, 1);
		TSTAMP_SPLX();
		return;
	}

	/*
	 * Verify the event is small enough to fit in the queue.
	 */
	nentries = 1 +
	    howmany(len - sizeof (TSTAMP_ENTRIES(pda)[0].qual),
		    sizeof (tstamp_event_entry_t));
	if (nentries > ts->nentries) {
		atomicAddInt(&ts->shared_state->lost_counter, 1);
		TSTAMP_SPLX();
		return;
	}

	/*
	 * Grab the space in the queue
	 */
	index = atomicAddWithWrap(
		&ts->shared_state->curr_index, nentries, ts->nentries);
	time = absolute_rtc_current_time();	/* AFTER getting index */
	event = &((TSTAMP_ENTRIES(pda))[index]);

	/*
	 * Check that the records we need are free and
	 * advance nevent to the last record we'll write.
	 *
	 * Note that curr_index was advanced above to reflect
	 * the number of entries we need.  If a slot past the
	 * first is still in use then we may leave a slot with
	 * a zero event type--the consumer must handle this
	 * special case by ignoring/discarding such records.
	 */
	n = nentries;
	nevent = event;
	for (;;) {
		/*
		 * The event reading thread marks events free by
		 * zeroing the evt field.  If any event record is
		 * still unread then drop the event we're to insert.
		 */
		if (nevent->evt != 0) {
			atomicAddInt(&ts->shared_state->lost_counter,1);
			TSTAMP_SPLX();
			return;
		}
		if (--n == 0)
			break;
		nevent = (nevent < TSTAMP_LAST(pda) ? (nevent + 1) :
				  TSTAMP_ENTRIES(pda));
	}

	/*
	 * Copy the event data.  We write extended events
	 * in contiguous space immediately following the
	 * first event's qual array (i.e. we leave no gaps
	 * between data).  The number of contiguous events
	 * used is recorded in the "jumbocnt" field of the
	 * first event.  Event readers (e.g. rtmond) are
	 * responsible for zeroing the event field of each
	 * extended event slot after reading the "jumbo
	 * event" so that each slot will appear free after
	 * the event is read.
	 */
	ASSERT(nevent != event);
	if (nevent < event) {
		/*
		 * We have two discontiguous segments, copy
		 * the event data in two operations.
		 */
		/* TSTAMP_LAST(pda) points to the last evt slot _in_ the buf */
		__scunsigned_t len1 = sizeof (event->qual)
			+ ((caddr_t)(TSTAMP_LAST(pda)+1) - (caddr_t)(event+1));
		bcopy(data, &event->qual, len1);
		bcopy(((caddr_t) data)+len1, TSTAMP_ENTRIES(pda), len-len1);
	} else {
		/*
		 * Got contiguous space in the queue; we
		 * can copy everything in one operation.
		 */
		bcopy(data, &event->qual, len);
	}

	/*
	 * Make sure that tstamps happen in order in the kernel
	 * queue.  They might get out of order if we are in the 
	 * process of logging something and an interrupt comes in
	 * after we have the index, but before we get the time.
	 */
	next_index = (index + nentries) % ts->nentries;
	if (next_index == ts->shared_state->curr_index) {
		event->tstamp = time;
	}
	else {
		nevent = (nevent < TSTAMP_LAST(pda) ? (nevent + 1) :
				  TSTAMP_ENTRIES(pda));
		if (nevent->evt == 0 ||	time < nevent->tstamp) {
			event->tstamp = time;
		} else {
			/*
			 * The 1 is just a value to make the current time
			 * less than the next timestamp.
			 */
			event->tstamp = nevent->tstamp - 1;
		}
	}

	event->jumbocnt = nentries-1;
	/*
	 * Mark the event type last because that is what the
	 * user-level process checks for to identify when a
	 * tstamp is complete.
	 */
	event->evt = evt;
	/*
	 * Increment the count of tstamps and check the fill
	 * state.  When the queue level reaches the water mark
	 * wakeup the monitoring process.  Avoid vsema calls at spl
	 * levels over splhi -- see comments in log_tsatmp_event().
	 */
	atomicAddInt(&ts->shared_state->tstamp_counter, nentries);

	if (ts->shared_state->tstamp_counter >= ts->water_mark &&
	    ts->nsleepers &&
	    evt != TSTAMP_EV_EODISP) {
		TSTAMP_SPLX();
		if (!issplprof(getsr())) {
			int s = splhi();

			ts = pda->p_tstamp_objp;
			if (ts != NULL &&
			    ts->shared_state != NULL &&
			    ts->shared_state->tstamp_counter >= ts->water_mark
			    &&
			    ts->nsleepers) {
				if (compare_and_dec_int_gt(&ts->nsleepers, 0))
					vsema(&ts->wmsema);
			}
			splx(s);
		}
	} else {
		TSTAMP_SPLX();
	}
}

void
tstamp_sample(int cpu)
{
	if (TEST_TSTAMP_OBJECT(cpu)) {
		tstamp_obj_t* ts = TSTAMP_OBJP(cpu);
		/*
		 * Users extract/dequeue from the head,
		 * We insert/enqueue at the tail
		 */
		if (ts->nsleepers) {
			if (compare_and_dec_int_gt(&ts->nsleepers, 0))
				vsema(&ts->wmsema);
		}
	}
}

#define ACQUIRE_TSTAMP_LOCK(cpu, how)				\
	ASSERT(TEST_CPU_OK(cpu));					\
	mrlock(&(pdaindr[cpu].pda->p_tstamp_lock), how, PZERO);
#define RELEASE_TSTAMP_LOCK(cpu)					\
	ASSERT(TEST_CPU_OK(cpu));					\
	mrunlock(&(pdaindr[cpu].pda->p_tstamp_lock));

int
tstamp_user_update(int cpu, int nread)
{
	if (!TEST_CPU_OK(cpu))
		return (EINVAL);
	
	ACQUIRE_TSTAMP_LOCK(cpu, MR_ACCESS);
	if (!TEST_TSTAMP_OBJECT(cpu)) {
		RELEASE_TSTAMP_LOCK(cpu);
		return (ENOENT);
	} else {
		tstamp_obj_t* ts = TSTAMP_OBJP(cpu);
		if (nread < 0 || nread > ts->shared_state->tstamp_counter) {
			RELEASE_TSTAMP_LOCK(cpu);		
			return (EINVAL);
		}
		
		atomicAddInt(&ts->shared_state->tstamp_counter, (-1 * nread));
		if (ts->shared_state->tstamp_counter < ts->nentries)
			ts->shared_state->lost_counter = 0;
		
		RELEASE_TSTAMP_LOCK(cpu);
		return (0);
	}
}

paddr_t
verify_mappable_tstamp(paddr_t buffer_addr, long len)
{
	int cpu;
	for (cpu = 0; cpu < numcpus; cpu++) {
		if (pdaindr[cpu].CpuId == -1)
			continue;
		ACQUIRE_TSTAMP_LOCK(cpu, MR_ACCESS);
		if (TEST_TSTAMP_OBJECT(cpu)) {
			tstamp_obj_t* ts = TSTAMP_OBJP(cpu);

			if (((paddr_t)ts->shared_state & 0x00000000ffffffff) ==
			    buffer_addr) {
				int mappable = (len <= (ts->nentries *
						sizeof(tstamp_event_entry_t) +
						TSTAMP_SHARED_STATE_LEN));
				RELEASE_TSTAMP_LOCK(cpu);
				return ((paddr_t)(mappable ? ts->shared_state
							   : 0));
			}
		}
		RELEASE_TSTAMP_LOCK(cpu);
	}
	return (0);
}

/* Make sure that info is in a known state */
void
tstamp_reset(tstamp_obj_t *ts, int flag)
{
	tstamp_event_entry_t *ev, *last;

	ts->shared_state->lost_counter = 0;	
	ts->shared_state->tstamp_counter = 0;
	/* The daemon needs to know the current index when entering stop
	   mode.  Since the buffer must be cleared anyway, it's simplest
	   just to zero the index. */
	ts->shared_state->curr_index = 0;

	ev = (tstamp_event_entry_t*)
	    ((__psunsigned_t)(ts->shared_state) + TSTAMP_SHARED_STATE_LEN);
	last = &ev[ts->nentries];
	while (ev <= last) {
		if (flag)
			ev->evt = 0;
		ev->jumbocnt = 0;
		ev++;
	}
}

void
tstamp_set_eob_mode(int cpu, uint mode, uint* omode)
{
	 tstamp_obj_t *ts;
	 uint64_t omask;

	/*
	 * NB: we assume the lock has already been acquired
	 */
	ASSERT(TSTAMP_OBJP(cpu) != 0);	

	ts = TSTAMP_OBJP(cpu);
	if (omode)
		*omode = TSTAMP_EOBMODE(cpu);

	if (mode != TSTAMP_EOBMODE(cpu)) {
		/*
		 * Temporarily disable logging while resetting the tstamp
		 * object.  We must wait long enough to know that any
		 * timestamp which was being recorded is now finished. 
		 */
		omask = pdaindr[cpu].pda->p_tstamp_mask;
		pdaindr[cpu].pda->p_tstamp_mask = 0;
		if (omask != 0)
			delay(1);			/* minimal delay */
		if (mode == RT_TSTAMP_EOB_STOP) {
			/* Entering stop mode:
			   clear all event names, reset index */
			tstamp_reset(ts, TRUE);
		}
		else {
			/* Entering wrap mode:
			   copy shared state->pda, clear jumbo evts */
			pdaindr[cpu].pda->p_tstamp_ptr =
				&TSTAMP_ENTRIES(pdaindr[cpu].pda)
						[ts->shared_state->curr_index];
			tstamp_reset(ts, FALSE);
		}
		/* must be set last:  controls whether or not fast path taken */
		TSTAMP_EOBMODE(cpu) = mode;
		pdaindr[cpu].pda->p_tstamp_mask = omask;
	}
}

/* Merge the utrace mask and the per-CPU rtmon mask (if any) */
void
tstamp_update_mask(int cpu)
{
	uint64_t mask = 0;

	/* NB:  Assumes that the lock is held by the caller */

	if (utrace_bufsize)			/* are UTRACEs enabled? */
		mask = (uint64_t)utrace_mask;
	if(TSTAMP_OBJP(cpu) != 0)
		mask |= TSTAMP_OBJP(cpu)->tracemask;
	pdaindr[cpu].pda->p_tstamp_mask = mask;
}

/*
 * user access
 */

int
tstamp_user_create(int cpu, int nentries, paddr_t* buffer_paddr)
{
	tstamp_obj_t* ts;

	if (!TEST_CPU_OK(cpu) ||
	    !(1 < nentries && nentries <= RT_TSTAMP_MAX_ENTRIES))
		return (EINVAL);

	ACQUIRE_TSTAMP_LOCK(cpu, MR_UPDATE);
	if (TEST_TSTAMP_OBJECT(cpu)) {		/* already exists */
		RELEASE_TSTAMP_LOCK(cpu);
		return (EEXIST);
	}
	ts = tstamp_construct(nentries, cpu);
	if (ts != (tstamp_obj_t*) 0) {		/* success, install */
		tstamp_install(ts, cpu);
		RELEASE_TSTAMP_LOCK(cpu);

		*buffer_paddr = tstamp_get_buffer_paddr(ts);
		return (0);
	} else {
		RELEASE_TSTAMP_LOCK(cpu);
		return (ENOMEM);
	}
}

int
tstamp_user_delete(int cpu)
{
	tstamp_obj_t* ts;

	if (!TEST_CPU_OK(cpu))
		return (EINVAL);
	/*
	 * If collecting data or UTRACEs are on we can not delete the queue
	 */
	if (pdaindr[cpu].pda->p_tstamp_mask || utrace_bufsize)
		return(EBUSY);
	ACQUIRE_TSTAMP_LOCK(cpu, MR_UPDATE);
	if (TEST_TSTAMP_OBJECT(cpu)) {
		/* Uninstall it so someone else does not try to use it */
		ts = tstamp_uninstall(cpu);
		tstamp_destruct(ts);
		RELEASE_TSTAMP_LOCK(cpu);

		return (0);				/* success */
	} else {
		RELEASE_TSTAMP_LOCK(cpu);
		return (EINVAL);
	}
}

int
tstamp_user_addr(int cpu, paddr_t* buffer_paddr)
{
	if (TEST_CPU_OK(cpu)) {
		ACQUIRE_TSTAMP_LOCK(cpu, MR_ACCESS);
		if (TEST_TSTAMP_OBJECT(cpu)) {
			*buffer_paddr = tstamp_get_buffer_paddr_from_cpu(cpu);
			RELEASE_TSTAMP_LOCK(cpu);
			return (0);			/* success */
		}
		RELEASE_TSTAMP_LOCK(cpu);
	}
	return (EINVAL);
}

int
tstamp_user_mask(int cpu, uint64_t mask, uint64_t* omask)
{
	if (TEST_CPU_OK(cpu)) {
		ACQUIRE_TSTAMP_LOCK(cpu, MR_UPDATE);
		if (TEST_TSTAMP_OBJECT(cpu)) {
			if (omask)
				*omask = TSTAMP_OBJP(cpu)->tracemask;
			TSTAMP_OBJP(cpu)->tracemask = mask;
			tstamp_update_mask(cpu);
			RELEASE_TSTAMP_LOCK(cpu);
			return (0);
		} else 
			RELEASE_TSTAMP_LOCK(cpu);	
	} 
	return (EINVAL);
}
   
int
tstamp_user_eob_mode(int cpu, uint mode, uint* omode)
{
	if (TEST_CPU_OK(cpu)) {
		if (mode == RT_TSTAMP_EOB_WRAP || mode == RT_TSTAMP_EOB_STOP) {
			ACQUIRE_TSTAMP_LOCK(cpu, MR_ACCESS);
			if (TEST_TSTAMP_OBJECT(cpu)) {
				tstamp_set_eob_mode(cpu, mode, omode);
				RELEASE_TSTAMP_LOCK(cpu);
				return(0);		/* success */
			}
			RELEASE_TSTAMP_LOCK(cpu);
		}
	}
	return (EINVAL);
}	

int
tstamp_user_wait(int cpu, int tout)
{
	if (TEST_CPU_OK(cpu)) {
	    if (TEST_TSTAMP_OBJECT(cpu)) {	/* tracing setup */
		    if (tout != 0) {		/* wait with timeout */
			    tstamp_obj_t* ts;
			    toid_t toid;
			    int ticks = (tout * HZ)/1000;
			    if (tout < 10 || ticks > 100000)
				    return (EINVAL);
			    toid = timeout(tstamp_sample,
				(void*)(__psint_t) cpu, ticks);
			    /*
			     * The lock is released (waking us up) by
			     * tstamp_sample when a timeout occurs or
			     * when an event is logged and the queue
			     * level reaches the specified watermark.
			     */
			    ASSERT(TSTAMP_OBJP(cpu) != 0);
			    ts = TSTAMP_OBJP(cpu);
			    atomicAddInt(&ts->nsleepers, 1);
			    if (psema(&ts->wmsema, PZERO+5)) {
				    compare_and_dec_int_gt(&ts->nsleepers, 0);
				    untimeout(toid);
				    return (EINTR);
			    }
		    } else {			/* wait indefinitely */
			    tstamp_sample(cpu);
		    }
		    return (0);			/* success */
	    }
	}
	return (EINVAL);
}



/*
 ****** UTRACE-specific code ******
 *
 * UTRACEs are a kernel tracing mechanism designed to be lightweight
 * enough to be used ubiquitously.  Having a log of recent events
 * is a great help when diagnosing customers' crashes.  The
 * current implementation uses rtmon's buffers and subroutines, with a
 * fast path in log_tstamp_event() which drastically decreases the
 * tracing overhead when rtmond is not active.  Icrash and idbg
 * contain commands which interleave and print the various CPUs'
 * events.  Generally the traces are not examined while the system is
 * running, but only during post-mortem debugging.
 */


/* 
 * This "sanity" routine is called by _debug_sanity() currently.  If we
 * ever decide that we want to give utrace its own systune category this
 * routine is ready to run as is.
 */
int
_utrace_sanity(
	void *address, 
	uint64_t arg)
{
	if (address == &utrace_mask) {
		int cpu;

		utrace_mask = (long long)arg;
		for(cpu = 0; cpu < maxcpus; cpu++) {
			if (TEST_CPU_OK(cpu)) {
				ACQUIRE_TSTAMP_LOCK(cpu, MR_UPDATE);
				tstamp_update_mask(cpu);
				RELEASE_TSTAMP_LOCK(cpu);
			}
		}
		return 0;
	}
	return 1;
}


/*
 * At boot time (just after PDAs are set up and memory allocation
 * works) initialize the trace buffers in wrap mode if utraces are
 * enabled.
 */
void
utrace_init()
{
	int cpu;
	paddr_t bufaddr;

	if (utrace_bufsize == 0)
		return;

	/* Until rtmond is fixed to obtain the buffer size from the
	   kernel, only one UTRACE buffer size is supported */
	if (utrace_bufsize != NUMB_KERNEL_TSTAMPS) {
		cmn_err(CE_WARN, 
				"utrace_bufsize has an invalid value (%d).  "
				"Resetting it to %d\n",
				utrace_bufsize, NUMB_KERNEL_TSTAMPS);
		utrace_bufsize = NUMB_KERNEL_TSTAMPS;
	}

	for (cpu=0; cpu<maxcpus; cpu++) {
		if (pdaindr[cpu].CpuId != -1) {
			if (tstamp_user_create(cpu, utrace_bufsize, &bufaddr) != 0)
				continue;
			tstamp_set_eob_mode(cpu, RT_TSTAMP_EOB_WRAP, NULL);			
			tstamp_update_mask(cpu);
		}
    }
}


/* 
 * Translate between rtmon-style event numbers and event names.
 *
 * This table is here (instead of with the other utrace code in
 * idbg.c) so it gets included in all kernels.  This is necessary for
 * icrash to translate events.
 */

/* ANSI-C trick to get each macro's name and value */
#define EVT(x) {(x), #x}
utrace_trtbl_t utrace_trtbl[] = {
/* Event names from rtmon.h.  XXX this is a crock */
    EVT(TSTAMP_EV_INTRENTRY),
    EVT(TSTAMP_EV_INTREXIT),
    EVT(TSTAMP_EV_EVINTRENTRY),
    EVT(TSTAMP_EV_EVINTREXIT),
    EVT(TSTAMP_EV_EODISP),
    EVT(TSTAMP_EV_EOSWITCH),
    EVT(TSTAMP_EV_EOSWITCH_RTPRI),
    EVT(TSTAMP_EV_START_OF_MAJOR),
    EVT(TSTAMP_EV_START_OF_MINOR),
    EVT(TSTAMP_EV_ULI),
    EVT(TSTAMP_EV_YIELD),
    EVT(TSTAMP_EV_CPUCOUNTER_INTR),
    EVT(TSTAMP_EV_RTCCOUNTER_INTR),
    EVT(TSTAMP_EV_TIMEOUT),
    EVT(TSTAMP_EV_PROFCOUNTER_INTR),
    EVT(TSTAMP_EV_GROUP_INTR),
    EVT(TSTAMP_EV_EOPRESUME),
    EVT(TSTAMP_EV_YIELD2),
    EVT(TSTAMP_EV_CPUINTR),
    EVT(TSTAMP_EV_NETINTR),
    EVT(TSTAMP_EV_VSYNC_INTR),
    EVT(TSTAMP_EV_XRUN),
    EVT(TSTAMP_EV_INTRQUAL),
    EVT(TSTAMP_EV_SIGSEND),
    EVT(TSTAMP_EV_SIGRECV),
    EVT(TSTAMP_EV_EXIT),
    EVT(TSTAMP_EV_TIMEIN),
    EVT(TSTAMP_EV_QUEUERUN),
    {TSTAMP_EV_PROF_STACK32, "PROF_STACK"}, /* PROF_STACK is #defined */
    {TSTAMP_EV_PROF_STACK64, "PROF_STACK"}, /* to one of these (prf.c) */
    EVT(TSTAMP_EV_JOB_MIGRATION),
    EVT(TSTAMP_EV_PROC_NAME),
    EVT(TSTAMP_EV_SYSCALL_BEGIN),
    EVT(TSTAMP_EV_SYSCALL_END),
    EVT(TSTAMP_EV_EXEC),
    EVT(TSTAMP_EV_CONFIG),
    EVT(TSTAMP_EV_TASKNAME),
    EVT(TSTAMP_EV_FORK),
    EVT(TSTAMP_EV_ALLOC),
    EVT(START_KERN_FUNCTION),
    EVT(END_KERN_FUNCTION),
    EVT(TSTAMP_EV_SORECORD),
  /* VM events - reserved KERNEL_EVENT(100) through KERNEL_EVENT(199) */
    EVT(VM_EVENT_TFAULT_ENTRY),
    EVT(VM_EVENT_TFAULT_EXIT),
    EVT(VM_EVENT_PFAULT_ENTRY),
    EVT(VM_EVENT_PFAULT_EXIT),
    EVT(VM_EVENT_PFAULT_RLACQ),
    EVT(VM_EVENT_PFAULT_NOTHV),
    EVT(VM_EVENT_PFAULT_ISMOD),
    EVT(VM_EVENT_PFAULT_STARTF),
    EVT(VM_EVENT_PFAULT_NOTCW),
    EVT(VM_EVENT_PFAULT_CW),
    EVT(VM_EVENT_VFAULT_ENTRY),
    EVT(VM_EVENT_VFAULT_EXIT),
    EVT(VM_EVENT_VFAULT_DFILLSTART),
    EVT(VM_EVENT_VFAULT_DFILLEND),
    EVT(VM_EVENT_VFAULT_RLACQ),
    EVT(VM_EVENT_MIGR_PLIST_ENTRY),
    EVT(VM_EVENT_MIGR_PLIST_EXIT),
    EVT(VM_EVENT_MIGR_PLIST_FAIL),
    EVT(VM_EVENT_MIGR_PMOVE_ENTRY),
    EVT(VM_EVENT_MIGR_PMOVE_EXIT),
    EVT(VM_EVENT_MIGR_PAGE_ENTRY),
    EVT(VM_EVENT_MIGR_PAGE_EXIT),
    EVT(VM_EVENT_MIGR_PAGE_FAIL),
    EVT(VM_EVENT_MIGR_FRAME_ENTRY),
    EVT(VM_EVENT_MIGR_FRAME_TLBSTART),
    EVT(VM_EVENT_MIGR_FRAME_TLBEND),
    EVT(VM_EVENT_MIGR_FRAME_CACHESTART),
    EVT(VM_EVENT_MIGR_FRAME_CACHEEND),
    EVT(VM_EVENT_MIGR_FRAME_EXIT),
    EVT(VM_EVENT_VFAULT_ANONINS),
    EVT(VM_EVENT_VFAULT_ADDMAP_START),
    EVT(VM_EVENT_VFAULT_ADDMAP_END),
    EVT(VM_EVENT_VFAULT_DROPIN),
  /* Scheduler event - reserved KERNEL_EVENT(200) through KERNEL_EVENT(299) */
    EVT(SCHED_EVENT_SAME_JOB),
    EVT(SCHED_EVENT_DIFFERENT_JOB),
    EVT(SCHED_EVENT_LOAD_BALANCE),
    EVT(SCHED_EVENT_VQFACT_SET),
    EVT(SCHED_EVENT_BAD_DATA),
  /* Disk i/o event - reserved KERNEL_EVENT(300) through KERNEL_EVENT(399) */
    EVT(DISK_EVENT_QUEUED),
    EVT(DISK_EVENT_START),
    EVT(DISK_EVENT_DONE),
  /* Network events - reserved KERNEL_EVENT(400) through KERNEL_EVENT(499) */
    EVT(NET_EVENT_NEW),
    EVT(NET_EVENT_SLEEP),
    EVT(NET_EVENT_WAKEUP),
    EVT(NET_EVENT_WAKING),
    EVT(NET_EVENT_FLOW),
    EVT(NET_EVENT_DROP),
    EVT(NET_EVENT_EVENT_DONE),
  /* Frame scheduling events - KERNEL_EVENT(8) through KERNEL_EVENT(15) */
    EVT(TSTAMP_EV_DETECTED_UNDERRUN),
    EVT(TSTAMP_EV_DETECTED_OVERRUN),
    EVT(TSTAMP_EV_RECV_NORECOVERY),
    EVT(TSTAMP_EV_RECV_INJECTFRAME),
    EVT(TSTAMP_EV_RECV_EFRAME_STRETCH),
    EVT(TSTAMP_EV_RECV_EFRAME_STEAL),
    EVT(TSTAMP_EV_RECV_TOOMANY),
  /* Rtmond (not the kernel) generates this when it drops events */
    EVT(TSTAMP_EV_LOST_TSTAMP),

/* More event names from wr_eventP.h (only these two are used directly) */
	EVT(EVENT_TASK_STATECHANGE),
	EVT(EVENT_WIND_EXIT_IDLE),

    {0, (char *)0}		/* Last entry has zeroes */
};
#undef EVT
