 /*
 * Copyright (c) 1997 Silicon Graphics, Inc.
 *
 * This file contains the Streams utility procedures specific to message and
 * buffer allocation/deallocation/manipulation for Streams. These procedures
 * are used by modules and drivers.
 *
 * The allocation and locking scheme in this implementation differ's from
 * previous IRIX releases and the vanilla SVR4 code. The current scheme has
 * been designed to use a the fast spin locks on platforms cover utilizing
 * the R4X00 series microprocessors and it's follow-on's who support the
 * MIPS III instruction set.
 *
 * The basic design is to use a set of linked lists, a copy of which is
 * maintained on each CPU. The vectors are stored in the per-cpu
 * private data area (PDA) structure, the address of which is stored in
 * PDA associated with each cpu.
 *
 * The objectives of the new scheme are:
 *
 * 1. Support fast allocation and deallocation of Streams msg/dblk/buffers
 * 2. Utilize fast spin locks which have been optimized for the R4X00 and
 *    it's successor microprocessors.
 * 3. Support both processor cache affinity and NUMA memory support to avoid
 *    accessing non-local memory on the MP NUMA systems and to avoid excessive
 *    secondary cache thrashing.
 * 4. Utilize a scheme which obtains whole pages of memory from the local
 *    memory attached to the node, so the processor access local memory most
 *    of the time. The pages are tiled, in powers of 2, for the specific
 *    node size required and maintained on the individual linked-lists.
 *    Utilize the kernel VM page allocator to obtain kernel pages local to
 *    the cpu if possible.
 * 5. The implementation supports a general scheme tro return nodes to the
 *    appropriate per-cpu the list where it was allocated by saving the cpuid
 *    in the msg/dblk headers. The cpuid for the buffer node is also saved
 *    in the dblk structure. This scheme avoids the problem on MP systems
 *    where nodes typically are allocated on one cpu and freed on a different
 *    one; Leading to unbalanced memory useage and requiring a recombination
 *    and/or recyling scheme be implemented.
 * 6. Support for Copy-on-Write avoidance and page exchange is NOT included
 *    but maybe implemented in the future in the stream head code IF we see
 *    a sweeping requirement for high performance third party Streams based
 *    networking protocols. None appear on the horizon at this point.
 */
#ident "$Revision: 3.67 $"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/debug.h"

#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/atomic_ops.h"

#include "sys/pfdat.h"
#include "sys/pda.h"
#include "sys/immu.h"
#include "sys/kmem.h"
#include "sys/sema.h"
#include "sys/cmn_err.h"
#include "sys/sysinfo.h"
#include <sys/conf.h>
#include <sys/idbgentry.h>

#include <sys/stream.h>
#include <sys/strmp.h>
#include <sys/strmdep.h>
#include <sys/strstat.h>
#include <sys/strsubr.h>

#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/idbgactlog.h>

#include <sys/kthread.h>

/*
 * DEBUG_FREE
 * If the above symbol is defined along with DEBUG TRUE, extra code is
 * compiled into the lf_free procedure to check for duplicate free operations
 * on the same mblk/dblk/buffer triple. This is implemented by maintaining
 * in the buffer list on each cpu an extra node list holding the current nodes
 * which have been freed and are being reclaimed. Compiling with DEBUG
 * defined is also required to get the extra data structures defined in
 * the various header files.
 */
 /* define DEBUG_FREE 1 */

 /*
 * Constants for various node size's for the vectors associated with each cpu.
 */
#define STR_MSG_INDEX		0 /* msgb and seinfo structures */
#define STR_MSG_SIZE		(sizeof(struct mbinfo))
#define	STR_MSG_THRESHOLD	(NBPP/STR_MSG_SIZE)
#define STR_MSG_PREALLOC	(NBPP/STR_MSG_SIZE)

#define STR_MD_INDEX		1
#define STR_MD_SIZE		(sizeof(struct mbinfo) +sizeof(struct dbinfo))
#define	STR_MD_THRESHOLD	(NBPP/STR_MD_SIZE)
#define STR_MD_PREALLOC		(NBPP/STR_MD_SIZE)

#define STR_BUF64_INDEX		2
#define STR_BUF64_SIZE		64
#define	STR_BUF64_THRESHOLD	(NBPP/STR_BUF64_SIZE)
#define STR_BUF64_PREALLOC	(NBPP/STR_BUF64_SIZE)

#define STR_BUF256_INDEX	3
#define STR_BUF256_SIZE		256
#define	STR_BUF256_THRESHOLD	(NBPP/STR_BUF256_SIZE)
#define STR_BUF256_PREALLOC	(NBPP/STR_BUF256_SIZE)

#define STR_BUF512_INDEX	4
#define STR_BUF512_SIZE		512
#define	STR_BUF512_THRESHOLD	(NBPP/STR_BUF512_SIZE)
#define STR_BUF512_PREALLOC	(NBPP/STR_BUF512_SIZE)

#define STR_BUF2K_INDEX		5
#define STR_BUF2K_SIZE		2048
#define	STR_BUF2K_THRESHOLD	(NBPP/STR_BUF2K_SIZE)
#define STR_BUF2K_PREALLOC	(NBPP/STR_BUF2K_SIZE)

#define STR_PAGE_INDEX		6
#define	STR_PAGE_THRESHOLD	2
#define STR_PAGE_PREALLOC	2

#define STR_MAX_INDEX		7

/*
 * Status flags for sleep blocking while getting VM pages
 */
#define S_WAIT		0	/* Wait for VM if required to come available */
#define S_DONTWAIT	1	/* Do NOT Wait for VM to come available */

#ifdef STREAMS_MEM_TRACE

#define	ALLOCEDBY	1
#define	DUPEDBY		2
#define	FREEDBY		3

#if _MIPS_SIM != _ABI64
void str_mem_trace(struct str_mem_trace_log *, int, void *, void *);
#define STR_MEM_TRACE(b,t,a1,a2) str_mem_trace(&((b)->tracelog),t,a1,a2)
#else
#define STR_MEM_TRACE(b,t,a1,a2)
#endif /* _MIPS_SIM != _ABI64 */

#else
#define STR_MEM_TRACE(b,t,a1,a2)

#endif /* STREAMS_MEM_TRACE */

/*
 * External Referenced Procedures
 */
extern void *kvpalloc_node_hint(cnodeid_t nodeid, u_int size, int flags, int color);
extern void *kvpalloc(u_int size, int flags,int color);
extern void kvpfree(void *vaddr, u_int size);

/*
 * Forward Referenced Procedures
 */
#ifdef DEBUG
void freeb_debug(mblk_t *, inst_t *);
#endif /* DEBUG */

void lf_addnode(struct listvec *, struct lf_elt *);
struct lf_elt *lf_addpage(struct lf_listvec *, int, struct lf_elt *);
struct lf_elt *lf_get(struct lf_listvec *, int, cpuid_t, int);
struct lf_elt *lf_getpage_tiled(int canwait, int nodesize, u_int vmflags,
	cpuid_t cpu_id);
void lf_free(struct lf_listvec *, int, struct lf_elt *, cpuid_t);
void lf_freepage(struct lf_elt *page);

void strgiveback(void);

void str_init_alloc_bufs(struct lf_listvec *lf_listvecp, cpuid_t cpu_id);
void str_init_lfvec(struct lf_listvec *lf_listvecp);
void str_init_masterlfvec(void);
void str_init_memsize(void);
void str_init_slave(struct lf_listvec *lf_listvecp, cpuid_t cpu_id,
	cnodeid_t node_id);
void str_preallocate(struct lf_listvec *, cpuid_t, int, int, int);
void str_stats_ops(cpuid_t, int, int, int);
int xmsgsize(mblk_t *, int);

/*
 * External Referenced Variables
 */
extern int str_num_pages; /* Max num pages for streams msg/dblk/buffs */
extern int str_min_pages; /* Mim num pages streams msg/dblk/buffs (read-only)*/
extern int maxcpus;       /* Num CPUs present = maxcpus - disabled CPUs */

extern char strbcflag;
extern char strbcwait;

/*
 * Global defined constants
 */
#define STR_MIN_PAGES	 16  /* Minimum number pages for all streams bufs */

/*
 * Major macro to get Streams list vectors local for a cpu
 */
#define STR_LFVEC(cpu) ((struct lf_listvec *)(pdaindr[cpu].pda->kstr_lfvecp))
#define STR_LFVEC_LOCAL(cpu) ((struct lf_listvec *)(private.kstr_lfvecp))
#define STRSTAT_CPU(cpu) ((struct strstat *)(pdaindr[cpu].pda->kstr_statsp))

/*
 * Stream procedure call entry/exit statistics
 * NOTE: All statistics related macro's are NO-OP's for production kernels,
 * statistics counts are maintained ONLY in DEBUG kernels.
 */
#ifdef DEBUG
struct str_procstats str_procstats;

#define STR_CALLSTATS(field) atomicAddUint(&(str_procstats.field), 1)
#define STR_CALLSTATS_INC(field, c) \
	atomicAddUint(&(str_procstats.field), c)
#define STR_LISTVECSTATS(lvp, field, c) \
	atomicAddUint(&((lvp)->field), c)
#else
#define STR_CALLSTATS(field)
#define STR_CALLSTATS_INC(field, c)
#define STR_LISTVECSTATS(lfp, field, c)
#endif /* DEBUG */

/*
 * Global list vectors for stream lists used by all cpu's.
 * We statically allocate the set for the master cpu since it is
 * too early in the startup sequence to call the kernel heap allocator.
 */
struct lf_listvec lf_mastercpu_localvec; /* master cpu only */

struct listvec templates_localvec[STR_MAX_INDEX] = { /* init values */

{0,0,0,0,0,-STR_MSG_SIZE,0,NBPP/STR_MSG_SIZE,STR_MSG_SIZE,STR_MSG_THRESHOLD},
{0,0,0,0,0,-STR_MD_SIZE,0,NBPP/STR_MD_SIZE,STR_MD_SIZE,STR_MD_THRESHOLD},
{0,0,0,0,0,-STR_BUF64_SIZE,0,NBPP/STR_BUF64_SIZE,STR_BUF64_SIZE,STR_BUF64_THRESHOLD},
{0,0,0,0,0,-STR_BUF256_SIZE,0,NBPP/STR_BUF256_SIZE,STR_BUF256_SIZE,STR_BUF256_THRESHOLD},
{0,0,0,0,0,-STR_BUF512_SIZE,0,NBPP/STR_BUF512_SIZE,STR_BUF512_SIZE,STR_BUF512_THRESHOLD},
{0,0,0,0,0,-STR_BUF2K_SIZE,0,NBPP/STR_BUF2K_SIZE,STR_BUF2K_SIZE,STR_BUF2K_THRESHOLD},
{0,0,0,0,0,-NBPP,0,1,NBPP,STR_PAGE_THRESHOLD}
};

int str_buffersizes[STR_MAX_INDEX] = {
	STR_MSG_SIZE, STR_MD_SIZE,
	STR_BUF64_SIZE, STR_BUF256_SIZE,
	STR_BUF512_SIZE, STR_BUF2K_SIZE, NBPP
};

struct strstat mastercpu_strstat; /* Stream stats block for master cpu only */

/*
 * Following global variables are the system limits for Streams page useage.
 * NOTE: All of the global variables are protected by the 'str_cnt_lock' lock.
 */
lock_t str_cnt_lock;	/* lock protecting global stream counts/variables */

int str_page_got;	/* Number of pages got from kernel allocator */
int str_page_rel;	/* Number of pages returned to kernel */

int str_page_cur;	/* Current number of pages allocated for streams */
int str_page_max;	/* Maximum number of pages for streams (read-only) */

time_t page_keep_lbolt;		/* keep page if lbot >= pagekeep */
time_t page_obtained_lbolt;	/* time last page obtained from page alloc */
time_t page_released_lbolt;	/* time last page returned to page alloc */

#define PAGE_KEEP_LBOLT	(HZ*10)	/* Keep new pages at least 'n' seconds */

/*
 * Streams page reclaim global variables
 */
int strgiveback_cpuid = 0;     /* cpu_id to use on next execution */
int strgiveback_token = 0;     /* token returned from timeout call */
int strgiveback_last_cpuid = 0; /* cpu_id used last time */

#ifdef DEBUG

int strgiveback_calls = 0; /* num calls to strgiveback() procedures */
time_t strgiveback_lbolt;      /* time last strgiveback proc executed */
int strgiveback_lastrel = 0;   /* num pages released last timeout */
int strgiveback_totrel = 0;    /* num pages released over time */
#endif /* DEBUG */

#define STRGIVEBACK_INIT (HZ*30) /* initialization timeout in seconds */
#define STRGIVEBACK_TO   (HZ*60) /* 1 cpu per freepage reclaim interval(secs)*/

/*
 * semaphore for user process'es waiting for streams memory pages
 */
sema_t str_page_sema;	/* Semaphore for waiting for a returned page */

/*
 * define the following symbol IF additional msg leak tracking is desired!
 * NOTE: DEBUG mode is also required!
 */
/* define DEBUG_LEAK 1 */

#if (DEBUG && DEBUG_LEAK)

#define DELETE_MSG_INUSE(x) delete_msg_inuse(x)
#define INSERT_MSG_INUSE(x) insert_msg_inuse(x)
/*
 * Routines for managing in-use lists : Defined ONLY with DEBUG compilation
 */
void
delete_msg_inuse(struct mbinfo *msgptr)
{
#ifdef DEBUG_LEAK
	int s;

	LOCK_STR_RESOURCE(s);
	if (msgptr->m_prev) { 
		msgptr->m_prev->m_next = msgptr->m_next; 
		if (msgptr->m_next) {
			msgptr->m_next->m_prev = msgptr->m_prev; 
		}
	} else { 
		Strinfo[DYN_MSGBLOCK].sd_head = (void *)(msgptr->m_next); 
		if (msgptr->m_next) {
			msgptr->m_next->m_prev = NULL;
		}
	} 
	Strinfo[DYN_MSGBLOCK].sd_cnt--;
	UNLOCK_STR_RESOURCE(s);
#endif /* DEBUG_LEAK */
	return;
}

void
insert_msg_inuse(struct mbinfo *msgptr)
{
#ifdef DEBUG_LEAK
	register int s;

	LOCK_STR_RESOURCE(s);
	msgptr->m_next = (struct mbinfo *)(Strinfo[DYN_MSGBLOCK].sd_head);
	Strinfo[DYN_MSGBLOCK].sd_head = (void *)(msgptr);
	if (msgptr->m_next) {
		msgptr->m_next->m_prev = (struct mbinfo *)msgptr;
	}
	msgptr->m_prev = NULL;
	Strinfo[DYN_MSGBLOCK].sd_cnt++;
	UNLOCK_STR_RESOURCE(s);
#else
	msgptr->m_next = msgptr->m_prev = NULL;
#endif /* DEBUG_LEAK */

	return;
}
#else /* (DEBUG && DEBUG_LEAK) */

#define DELETE_MSG_INUSE(x)
#define INSERT_MSG_INUSE(x)

#endif /* (DEBUG && DEBUG_LEAK) */


#ifdef STREAMS_MEM_TRACE
void
str_mem_trace(struct str_mem_trace_log *logp, int t, void *a1, void *a2)
{
#if _MIPS_SIM != _ABI64
	register int idx;

	idx = logp->woff % MEM_TRACE_LOG_SIZE;
	logp->woff++;
	logp->entry[idx].type = t;
	logp->entry[idx].arg1 = a1;
	logp->entry[idx].arg2 = a2;
	logp->entry[idx].te_pad = 0;
#endif /* _MIPS_SIM != _ABI64 */
	return;
}

void
str_mem_trace_print(struct str_mem_trace_log *logp, char *pfx)
{
#if _MIPS_SIM != _ABI64
	register int i, j;

	for (i = 0, j = logp->woff % MEM_TRACE_LOG_SIZE;
	     i < MEM_TRACE_LOG_SIZE;
	     i++, j = ++j % MEM_TRACE_LOG_SIZE) {
		qprintf("%stracelog[%d]: type %d a1 0x%x a2 0x%x\n",
			pfx, j,
		        logp->entry[j].type,
		        logp->entry[j].arg1,
		        logp->entry[j].arg2);
	}
#endif /* _MIPS_SIM != _ABI64 */
	return;
}
#endif /* STREAMS_MEM_TRACE */

/*
 * Procedure Description:
 *   Add a page of nodes to the local list vector and return a node.
 *
 * Parameter Description:
 *   lf_listvecp => Address of the local listvec structure
 *
 * Returns:
 *   Address of the node.
 *
 * NOTE: Assumes full vm page passed in 'page' has already been tiled
 */
struct lf_elt *
lf_addpage(lf_listvecp, index, page)
	struct lf_listvec *lf_listvecp;
	int index;
	struct lf_elt *page;
{
	register struct lf_elt *lastnode;
	register int s_lfvec;

	STR_CALLSTATS(lf_addpage_calls);

	s_lfvec = mutex_spinlock(&lf_listvecp->lock);

	lastnode = (struct lf_elt *)(((char *)page) + NBPP -
			lf_listvecp->buffer[index].size);
	/*
	 * got a new page so add all the nodes except the first one
	 * to the tail of the existing free list. We must check in the off
	 * chance that somebody may have already added something to the
	 * head of the list. In this case we append the new page of nodes
	 * onto the new tail.
	 */
	if (lf_listvecp->buffer[index].head == NULL) { /* list still empty */

		lf_listvecp->buffer[index].head = page->next;
		lf_listvecp->buffer[index].tail = lastnode;

		STR_CALLSTATS(lf_addpage_empty);
	} else {

		lf_listvecp->buffer[index].tail->next = page->next;
		lf_listvecp->buffer[index].tail = lastnode;

		STR_CALLSTATS(lf_addpage_occupied);
	}
	lf_listvecp->buffer[index].nodecnt =
		lf_listvecp->buffer[index].e_per_page - 1; 

	mutex_spinunlock(&lf_listvecp->lock, s_lfvec);
	return page;
}

/*
 * Procedure Description:
 * Get procedure returns a node from the designed list vector structure
 *
 * Parameters:
 *   lf_listvecp => Address of local list vector structure
 *
 * Returns:
 *   e => Address of node to return
 */
struct lf_elt *
lf_get(lf_listvecp, index, cpu_id, canwait)
	struct lf_listvec *lf_listvecp;
	int index;
	cpuid_t cpu_id;
	int canwait;
{
	struct listvec *listvecp;
	struct lf_elt *e, *p;
	int rsize, s_lfvec;

	listvecp = &(lf_listvecp->buffer[index]);

	STR_CALLSTATS(lf_get_calls);
	STR_LISTVECSTATS(listvecp, get_calls, 1);

	s_lfvec = mutex_spinlock(&lf_listvecp->lock);

	if ((e = listvecp->head)) {

		listvecp->head = e->next;
		if (listvecp->head == NULL) {
			listvecp->tail = NULL;
		}
		listvecp->nodecnt--;
		mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

		STR_LISTVECSTATS(listvecp, get_local, 1);
		STR_CALLSTATS(lf_get_local);

	} else { /* failed to get node */

		ASSERT(listvecp->tail == NULL);
		mutex_spinunlock(&lf_listvecp->lock, s_lfvec);
		/*
		 * Get an allocated page of memory and tile it
		 */
		rsize = listvecp->size;

		if (p = (struct lf_elt *)lf_getpage_tiled(canwait,
				rsize, VM_DIRECT|VM_STALE, cpu_id)) {

			if (rsize < NBPP) {
				/*
				 * Node size less than an entire page so add
				 * the page of linked nodes safely to the get
				 * list and return the first node.
				 */
				e = lf_addpage(lf_listvecp, index, p);
			} else { /* full page size requested */
				e = p;
			}
			STR_LISTVECSTATS(listvecp, get_addpage, 1);
			STR_CALLSTATS(lf_get_addpage);
		} else { /* failed */
			e = (struct lf_elt *)NULL;

			STR_LISTVECSTATS(listvecp, get_fail, 1);
			STR_CALLSTATS(lf_get_fail);
		}
	}
	return e;
}

/*
 * Procedure Description:
 *   Get a page from the kernel and tile it with equal sized nodes.
 *
 * Parameter Description:
 *   canwait => flags meaning:
 *		S_WAIT => ok to wait if necessary
 *		S_DONTWAIT => Do NOT wait
 *   nodesize  => Size of node in bytes
 *   vmflags => vm flags for page type, coloring, etc.
 *   cpu_id => cpu id on which were executing
 *
 * Returns:
 *   Address of the allocated page already tiled.
 */
/* ARGSUSED */
struct lf_elt *
lf_getpage_tiled(canwait, nodesize, vmflags, cpu_id)
	int canwait;
	int nodesize;
	u_int vmflags;
	cpuid_t cpu_id;
{
	struct lf_listvec *lf_listvecp;
	struct listvec *listvecp;
	struct lf_elt *page, *current;
	char *next;
	u_int flags, n;
	int s_lfvec;

	STR_CALLSTATS(lf_getpage_tiled_calls);
	/*
	 * See if we have free pages on the page size buffer list for
	 * this cpu before getting one from the system. So the page list
	 * is acting both as a buffer pool and a reclaim list for extra pages.
	 */
	lf_listvecp = STR_LFVEC(cpu_id);
	listvecp = &(lf_listvecp->buffer[STR_PAGE_INDEX]);
retry:
	s_lfvec = mutex_spinlock(&lf_listvecp->lock);

	if ((page = listvecp->head)) {

		listvecp->head = page->next;
		if (listvecp->head == NULL) {
			listvecp->tail = NULL;
		}
		listvecp->nodecnt--;

		mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

		STR_CALLSTATS(lf_getpage_tiled_reclaim);

		if (nodesize < NBPP) {
			/* tile the page with linked buffers */
			n = (NBPP/nodesize) - 1;
			for (current = page, next = (char *)current; n; n--) {
				next += nodesize;
				current->next = (struct lf_elt *)next;
				current = (struct lf_elt *)next;
			}
			current->next = (struct lf_elt *)0;
		} else {
			page->next = (struct lf_elt *)0;
		}
		return page;
	}
	mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

	/*
	 * We don't acquire the str_cnt_lock lock to test 'str_page_cur'
	 * since we must to release it to allocate a page from the kernel.
	 * Hence str_page_cur and str_page_max could change between the check
	 * and the following update.
	 * Check if we're under maximum number of pages allowed
	 */
	if (str_page_cur < str_page_max) { /* ok */
		/*
		 * don't hold lock over kernel proc which can sleep
		 * mutex_spinunlock(&str_cnt_lock, s); not needed
		 */
		flags = vmflags;
		if (canwait == S_DONTWAIT) {
			flags |= VM_NOSLEEP;
		}
		/*
		 * Specify node where to get kernel vm page
		 */
		page = (struct lf_elt *)kvpalloc_node_hint((cputocnode(cpu_id)), 1,
					(int)flags, 0);

		if (page == NULL) { /* failed */
			STR_CALLSTATS(lf_getpage_tiled_failed);
			return ((struct lf_elt *)0);
		}
		STR_CALLSTATS(lf_getpage_tiled_newpage);

		/* update current number of pages we have for streams */
		atomicAddInt(&str_page_cur, 1);

		/* update number of pages we've ever got for streams */
		atomicAddInt(&str_page_got, 1);

		/*
		 * Note last time we obtained a page from kernel and
		 * compute next time to allow a page to be released
		 */
		page_obtained_lbolt = lbolt;
		page_keep_lbolt = page_obtained_lbolt + PAGE_KEEP_LBOLT;
	} else {
		/*
		 * Exceeded maximum number of pages allowed
		 * So failing that wait if allowed until one is freed.
		 */
		if (canwait == S_WAIT) {
			STR_CALLSTATS(lf_getpage_tiled_maxwait);
			psema(&str_page_sema, PZERO-1);
			goto retry;
		} else {
			STR_CALLSTATS(lf_getpage_tiled_maxnowait);
		}
		/* fail if can't wait and over limit */
		return ((struct lf_elt *)0);
	}
	if (nodesize < NBPP) {
		/*
		 * Now tile the page with nodes of the specified size
		 */
		n = (NBPP/nodesize) - 1;
		for (current = page, next = (char *)current; n; n--) {
			next += nodesize;
			current->next = (struct lf_elt *)next;
			current = (struct lf_elt *)next;
		}
		current->next = (struct lf_elt *)0;
	} else {
		page->next = (struct lf_elt *)0;
	}
	return page;
}

#ifdef STREAMS_DEBUG
/*
 * Procedure Description:
 *  The generic Streams put nxt procedures which calls the next 'put'
 *  procedures with the message and the next pointer.
 *  NOTE: This procedure is ONLY used in rare cases where various storage
 *  leaks need to be isolated.
 *
 * Parameter Description:
 *   q => Address of the Streams queue structure
 *   mp => Address of the message block
 *
 * Returns:
 *   Void
 */
void
putnext_debug(queue_t *q, mblk_t *mp)
{
	SAVE_PUTNEXT_Q_ADDRESS(mp, q);
	SAVE_PUTNEXT_QIPUTP_ADDRESS(mp, ((q)->q_next->q_qinfo->qi_putp));
	((*(q)->q_next->q_qinfo->qi_putp)((q)->q_next, (mp)));
}
#endif /* STREAMS_DEBUG */

/*
 * Procedure Description:
 *   Add a buffer to the free list specified
 *
 * Parameters:
 *   listvecp => Address of the list head
 *   page => Address of node to add onto free list specified
 *
 * Returns:
 *   Void
 */
void
lf_addnode(listvecp, page)
	struct listvec *listvecp;
	struct lf_elt *page;
{
	/* add page size node to tail of free list */
	page->next = NULL;

	if (listvecp->head == NULL) {
		listvecp->head = listvecp->tail = page;
	} else {
		listvecp->tail->next = page;
		listvecp->tail = page;
	}
	listvecp->nodecnt++;
	return;
}

/*
 * Procedure Description:
 *   Return a node to the designated listvec spcified by 'index'
 *
 * Parameter Description:
 *   lf_listvecp => Address of list head structure
 *   index => Index of buffer inside list head structure to return node
 *   e => Address of node to return
 *   cpu_id => cpu id where to return node and/or pages if required
 *
 * Returns:
 *   Void
 */
/* ARGSUSED */
void
lf_free(lf_listvecp, index, e, cpu_id)
	struct lf_listvec *lf_listvecp;
	int index;
	struct lf_elt *e;
	cpuid_t cpu_id;
{
	struct listvec *listvecp;
	register int s_lfvec;
	register caddr_t page;
#ifdef DEBUG
	register __psunsigned_t invalid_addr;
#endif /* DEBUG */

#ifdef DEBUG_FREE
	struct lf_elt *freenode_cur;
#endif /* DEBUG_FREE */

	listvecp = &(lf_listvecp->buffer[index]);
	STR_CALLSTATS(lf_free_calls);
	STR_LISTVECSTATS(listvecp, free_calls, 1);

	if (e == (struct lf_elt *)0) { /* null element address */
		return;
	}
	/* compute base page address associated with node */
	page = (caddr_t)((__psunsigned_t)e & -NBPP);

	/* claim local list lock */
	s_lfvec = mutex_spinlock(&lf_listvecp->lock);

	if (listvecp->e_per_page != 1) { /* NOT page size nodes */
		/*
		 * Node size not NBPP so check if reclaim page address set
		 * Set reclaim page address if it's null, then return.
		 */
		if (listvecp->reclaim_page == (char *)NULL) {

			listvecp->reclaim_page = page;
#if (DEBUG && DEBUG_FREE)
			e->next = NULL;
			listvecp->reclaim_page_cur = e;
#endif /* DEBUG && DEBUG_FREE */

			listvecp->nelts = 1;
			mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

			STR_LISTVECSTATS(listvecp, free_reclaimpage_set, 1);
			STR_CALLSTATS(lf_free_reclaimpage_set);
			return;
		}

		if (listvecp->reclaim_page == page) {

#if (DEBUG && DEBUG_FREE)
			freenode_cur = listvecp->reclaim_page_cur;
			while (freenode_cur) {
				/*
				 * check for freeing node twice OR
				 * node is not in the reclaim page
				 */
				if ((((__psunsigned_t)freenode_cur & -NBPP) !=
				     (__psunsigned_t)page) ||
				      (freenode_cur == e)) {
					cmn_err(CE_PANIC,
"lf_free: Duplicate free lf_listvecp 0x%x, index %d, e 0x%x, cpuid %d\n",
					lf_listvecp, index, e, cpu_id);
				} else {
					freenode_cur = freenode_cur->next;
				}
			}
#endif /* DEBUG && DEBUG_FREE */

			listvecp->nelts++;
			if (listvecp->nelts >= listvecp->e_per_page) {

				listvecp->nelts = 0;
				listvecp->reclaim_page = (char *)NULL;
#if (DEBUG && DEBUG_FREE)
				listvecp->reclaim_page_cur =
					(struct lf_elt *)NULL;
#endif /* DEBUG && DEBUG_FREE */

#ifdef DEBUG
				listvecp->free_reclaimpage_incr = 0;
#endif /* DEBUG */
				if (str_page_cur >= str_page_max) {

					mutex_spinunlock(&lf_listvecp->lock,
						s_lfvec);

					/* free immediately as over limit */
					lf_freepage((struct lf_elt *)page);
				} else {
	
				   /* add to free page list on this cpu */
				   lf_addnode(
					&(lf_listvecp->buffer[STR_PAGE_INDEX]),
					(struct lf_elt *)page);

					mutex_spinunlock(&lf_listvecp->lock,
						s_lfvec);
				}
				STR_LISTVECSTATS(listvecp,
					free_reclaimpage_clear, 1);
				STR_CALLSTATS(lf_free_reclaimpage_clear);
			} else {

#if (DEBUG && DEBUG_FREE)
				e->next = listvecp->reclaim_page_cur;
				listvecp->reclaim_page_cur = e;
#endif /* DEBUG && DEBUG_FREE */
				mutex_spinunlock(&lf_listvecp->lock, s_lfvec);
				STR_LISTVECSTATS(listvecp,
					free_reclaimpage_incr,1);
				STR_CALLSTATS(lf_free_reclaimpage_incr);
			}
			return;
		}
	} else { /* page size nodes */

		if (str_page_cur >= str_page_max) { /* free immediately */

			mutex_spinunlock(&lf_listvecp->lock, s_lfvec);
			lf_freepage((struct lf_elt *)page);

			STR_LISTVECSTATS(listvecp, free_pagesize_release, 1);
			STR_CALLSTATS(lf_free_pagesize_release);
			return;
		}
		/* fall through case will add it to the free list */
	}

#ifdef DEBUG
	/*
	 * NOTE: Since all node sizes are a power of two we check in
	 * debug mode that the appropriate bottom address bits are zero
	 * and fail if they are non-zero.
	 */
	invalid_addr = (__psunsigned_t)e & ~listvecp->node_mask;

	if (invalid_addr) { /* bad address not proper power of 2 */
		cmn_err(CE_PANIC,
		"lf_free: Bad free addr 0x%x, node_mask 0x%x, lfvec 0x%x\n",
		  (__psunsigned_t)e, listvecp->node_mask, lf_listvecp);

		mutex_spinunlock(&lf_listvecp->lock, s_lfvec);
		return;
	}
#endif /* DEBUG */

	/* 
	 * Add node to tail of free list and increment node count
	 */
	e->next = NULL;
	if (listvecp->head == NULL) {
		listvecp->head = listvecp->tail = e;
	} else {
		listvecp->tail->next = e;
		listvecp->tail = e;
	}
	listvecp->nodecnt++;

	mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

	STR_LISTVECSTATS(listvecp, free_localadd, 1);
	STR_CALLSTATS(lf_free_localadd);
	return;
}

/*
 * Procedure Description:
 *   Free a page buffer by returning it to the kernel VM allocator
 *
 * Parameters:
 *   page => Address of page size buffer to return
 *
 * Returns:
 *   Void
 */
void
lf_freepage(struct lf_elt *page)
{
	STR_CALLSTATS(lf_freepage_calls);

	/* Update current page count and number pages released */
	atomicAddInt(&str_page_cur, -1);
	atomicAddInt(&str_page_rel, 1);

	kvpfree(page, 1);

	/* compute next time to allow page release */
	page_released_lbolt = lbolt;
	page_keep_lbolt = page_released_lbolt + PAGE_KEEP_LBOLT;

	while (cvsema(&str_page_sema)) ;
	STR_CALLSTATS(lf_freepage_release);
	return;
}

/*
 * Procedure Description:
 *   Preallocate the streams pages to the various list heads.
 *
 * Parameters:
 *   lf_listvecp => Address of the listvector to add tiled nodes.
 *		return to kernel VM allocator
 *   cpu_id => Designated cpu where page was originally allocated
 *		return to kernel VM allocator
 *   min_alloc => Minimum number of nodes to add
 *   index => Array index in listvector for the appropriate area
 *   size => Node size in bytes
 *
 * Returns:
 *   Void
 */
void
str_preallocate(lf_listvecp, cpu_id, min_alloc, index, size)
	struct lf_listvec *lf_listvecp;
	cpuid_t cpu_id;
	int min_alloc;
	int index;
	int size;
{
	register struct lf_elt *lastnode, *page;
	register u_short i;

	/*
	 * Preallocate page, tile with nodes of the requested size and
	 * add to the head of the designated free list.
	 */
	i = 0;
	while (i < min_alloc) {

		if (page = (struct lf_elt *)lf_getpage_tiled(S_DONTWAIT, size,
				VM_DIRECT|VM_STALE, cpu_id)) {
			/*
			 * ok got full page for buffers so link onto free list
			 * ASSUMES last node in page has it's next field NULL
			 */
			lastnode = (struct lf_elt *)((char *)page +NBPP -size);

			if (lf_listvecp->buffer[index].head) { /* tail add */

				lf_listvecp->buffer[index].tail->next = page;
			} else { /* set list head and tail initially */

				lf_listvecp->buffer[index].head = page;
				lf_listvecp->buffer[index].tail = lastnode;
			}
			lf_listvecp->buffer[index].nodecnt +=
				lf_listvecp->buffer[index].e_per_page;

			i += lf_listvecp->buffer[index].e_per_page;

		} else { /* failed */
			break;
		}
	}
	return;
}

/*
 * Procedure Description:
 *   Direct the preallocation of streams buffers to the various list heads.
 *
 * Parameters:
 *   lf_listvecp => Address of the listvector to add tiled nodes.
 *		return to kernel VM allocator
 *   cpu_id => Designated cpu to initialize
 *
 * Returns:
 *   Void
 */
void
str_init_alloc_bufs(struct lf_listvec *lf_listvecp, cpuid_t cpu_id)
{
	/* Preallocate some space for msg blocks */
	str_preallocate(lf_listvecp, cpu_id, STR_MSG_PREALLOC,
		STR_MSG_INDEX, STR_MSG_SIZE);

	/* Preallocate some space for combined msg/datab blocks */
	str_preallocate(lf_listvecp, cpu_id, STR_MD_PREALLOC,
		STR_MD_INDEX, STR_MD_SIZE);

	/* Preallocate some space for various buffer sizes */
	str_preallocate(lf_listvecp, cpu_id, STR_BUF64_PREALLOC,
		STR_BUF64_INDEX, STR_BUF64_SIZE);

	str_preallocate(lf_listvecp, cpu_id, STR_BUF256_PREALLOC,
		STR_BUF256_INDEX, STR_BUF256_SIZE);
	/*
	 * NOTE: We don't preallocate some space for the larger buffers
	 * since they aren't heavily used in most machines.
	 * This they are required then the storage is dynamically allocated
	 * and the nodes linked onto the appropriate lists. Those include the
	 * STR_BUF512_SIZE, STR_BUF2K_SIZE and NBPP buffer sizes.
	 */
	return;
}

/*
 * Procedure Description:
 *  Allocate and initialize the stream list vectors local to this cpu.
 *  Called during the initialization of a slave cpu AFTER the knetvec
 *  (kernel network vector area) has been allocated and the wired tlb entry
 *  for the private pda area has been set for this cpu.
 *
 * Returns:
 *  Void
 */
void
str_init_lfvec(struct lf_listvec *lf_listvecp)
{
	int ix;

	bzero(lf_listvecp, sizeof(struct lf_listvec));
	spinlock_init(&(lf_listvecp->lock), 0);

	for (ix=0; ix < STR_MAX_INDEX; ix++) {
		lf_listvecp->buffer[ix] = templates_localvec[ix];
	}
	return;
}

/*
 * Procedure Description:
 *  Initialize the network buffer data structures for the master cpu.
 *  Called during the initialization of the master cpu AFTER the knetvec
 *  (kernel network vector area) has been allocated. For the master
 *  cpu the knstr structure is global since the kernel heap allocator
 *  hasn't been initialized at that time of the call to this procedure.
 *
 * Returns:
 *   Zero => Success
 *   Non-Zero => Failure
 */
/* ARGSUSED */
int
str_init_master(cpuid_t cpu_id)
{
	/* Initialize stream list vectors local for the master cpu */
	str_init_masterlfvec();

	/*
	 * NOTE: We can't allocate a set of tile pages for streams
	 * mblk/dblk/buffers for the master cpu at this point as the vm
	 * system has not been totally initialized. So we simply initialize
	 * the vector structures themselves. This means the master cpu's
	 * list vectors filled on initial use.
	 */
	bzero(&mastercpu_strstat, sizeof(struct strstat));
#ifdef DEBUG
	bzero(&str_procstats, sizeof(struct str_procstats));
#endif
	pdaindr[cpu_id].pda->kstr_lfvecp = (char *)&lf_mastercpu_localvec;
	pdaindr[cpu_id].pda->kstr_statsp = (char *)&mastercpu_strstat;
	/*
	 * Initialize:
	 * 1. global stream lock protecting the global stream count variables
	 * 2. global stream need a page semaphore
	 * 3. global lbolt times for page_obtained, page_released and page_keep
	 */
	spinlock_init(&str_cnt_lock, 0);
	initnsema(&str_page_sema, 0, "strbufpg");

	page_obtained_lbolt = page_released_lbolt = page_keep_lbolt = 0;
	/*
	 * The validation of the stream tuning parameters is done in the
	 * startup code for the master cpu when it is initialized. Hence
	 * a call to 'str_init_memsize()' here isn't valid as physical
	 * memory isn't sized until after this procedure is called.
	 */
	return 0;
}

/*
 * Procedure Description:
 *  Initialize the global network buffer parameters specific to the
 *  memory size of the machine. This is called during the initialization
 *  of the master cpu but AFTER the physical memory size of the machine
 *  has been determined.
 *
 *  NOTE: This procedure requires that the global 'physmem' variable
  *  be set to a valid value upon entry.
 *
 * Returns:
 *   Void
 */
void
str_init_memsize(void)
{
	/*
	 * Validate the streams buffers tuning parameters potentially
	 * set/overridden by the system administrator. In units of pages.
	 */
	if (str_num_pages == 0) { /* autoconfig max to 1/8 of physical mem */
		str_num_pages = physmem / 8;
	}
	if (str_min_pages == 0) { /* autoconfig min to 1/256 of physical mem */
		str_min_pages = physmem / 256;
	}
	if (str_min_pages < STR_MIN_PAGES) { /* set to minimum if too small */
		str_min_pages = STR_MIN_PAGES;
	}
	if (str_min_pages >= str_num_pages) { /* clamp if too big */
		str_min_pages = str_num_pages - 1;
	}
	str_page_max = str_num_pages;
	return;
}

/*
 * Procedure Description:
 *  Allocate and initialize the stream list vectors local to this cpu.
 *  Called during the initialization of a slave cpu AFTER the knstr
 *  (kernel network activity area) has been allocated and the wired tlb entry
 *  for the private pda area has been set for this cpu.
 *
 * Returns:
 *  Zero => Failure
 *  Non-Zero => Address of the lf_listvec structure
 */
/* ARGSUSED */
struct lf_listvec *
str_init_alloc_lfvec(cpuid_t cpu_id)
{
	register struct lf_listvec *lf_listvecp;
	cnodeid_t nodeid = cputocnode(cpu_id);

	if ((lf_listvecp = (struct lf_listvec *)kern_calloc_node(1,
			sizeof(struct lf_listvec), nodeid))) {
		bzero(lf_listvecp, sizeof(struct lf_listvec));
	}
	return lf_listvecp;
}

/*
 * Procedure Description:
 *  Allocate and initialize the stream statistics block local to this cpu.
 *  Called during the initialization of a slave cpu AFTER the knstr
 *  (kernel network activity area) has been allocated and the wired tlb entry
 *  for the private pda area has been set for this cpu.
 *
 * Returns:
 *  Zero => Failure
 *  Non-Zero => Address of the strstat structure
 */
/* ARGSUSED */
struct strstat *
str_init_alloc_strstat(cpuid_t cpu_id)
{
	register struct strstat *strstatp;
	cnodeid_t nodeid = cputocnode(cpu_id);

	if ((strstatp = (struct strstat *)kern_calloc_node(1,
			sizeof(struct strstat), nodeid))) {
		bzero(strstatp, sizeof(struct strstat));
	}
	return strstatp;
}

/*
 * Procedure Description:
 *  Initialize the stream list vectors local to this cpu.
 *  Called during the initialization of a slave cpu AFTER the knstr
 *  (kernel network activity area) has been allocated and the wired tlb entry
 *  for the private pda area has been set for this cpu.
 *
 * Returns:
 *  void
 */
void
str_init_masterlfvec(void)
{
	int ix;
	spinlock_init(&(lf_mastercpu_localvec.lock), 0);

	for (ix=0; ix < STR_MAX_INDEX; ix++) {
		lf_mastercpu_localvec.buffer[ix] = templates_localvec[ix];
	}
	return;
}

/*
 * Procedure Description:
 *  Initialize the network buffer data structures for a slave cpu.
 *  Called during the initialization of the master cpu AFTER the knstr
 *  (kernel network activity area) has been allocated and the wired tlb entry
 *  for the private pda area has been set for this cpu.
 *
 * Returns:
 *  void
 */
/* ARGSUSED */
void
str_init_slave(lf_listvecp, cpu_id, node_id)
	struct lf_listvec *lf_listvecp;
	cpuid_t cpu_id;
	cnodeid_t node_id;
{
	/* Initialize the local list vectors for this cpu */
	str_init_lfvec(lf_listvecp);

	/* Allocate stream msg/dblk and buffers for this cpu */
	str_init_alloc_bufs(lf_listvecp, cpu_id);
	return;
}

void
str_stats_ops(cpuid_t cpu_id, int index, int op, int c)
{
	struct strstat *strstatp = STRSTAT_CPU(cpu_id);
	ASSERT(strstatp != 0);

	switch (op) {

	case STR_STREAM_OPS:
		atomicAddInt(&(strstatp->stream.use),c);
		if (strstatp->stream.use > strstatp->stream.max) {
			strstatp->stream.max = strstatp->stream.use;
		}
		break;

	case STR_STREAM_FAILOPS:
		atomicAddInt(&(strstatp->stream.fail),c);
		break;

	case STR_QUEUE_OPS:
		atomicAddInt(&(strstatp->queue.use),c);
		if (strstatp->queue.use > strstatp->queue.max) {
			strstatp->queue.max = strstatp->queue.use;
		}
		break;

	case STR_QUEUE_FAILOPS:
		atomicAddInt(&(strstatp->queue.fail),c);
		break;

	case STR_MD_BUF_OPS:
		atomicAddInt(&(strstatp->buffer[STR_MD_INDEX].use), 1);
		if (strstatp->buffer[STR_MD_INDEX].use >
		    strstatp->buffer[STR_MD_INDEX].max) {

			strstatp->buffer[STR_MD_INDEX].max =
				strstatp->buffer[STR_MD_INDEX].use;
		}
		ASSERT((index >= STR_MSG_INDEX) && (index <= STR_PAGE_INDEX));

		atomicAddInt(&(strstatp->buffer[index].use), c);
		if (strstatp->buffer[index].use > strstatp->buffer[index].max){
			strstatp->buffer[index].max =
				strstatp->buffer[index].use;
		}
		break;

	case STR_BUFFER_OPS:
		ASSERT((index >= STR_MSG_INDEX) && (index <= STR_PAGE_INDEX));

		atomicAddInt(&(strstatp->buffer[index].use),c);
		if (strstatp->buffer[index].use > strstatp->buffer[index].max){
			strstatp->buffer[index].max =
				strstatp->buffer[index].use;
		}
		break;

	case STR_BUFFER_FAILOPS:
		ASSERT((index >= STR_MSG_INDEX) && (index <= STR_PAGE_INDEX));
		atomicAddInt(&(strstatp->buffer[index].fail),c);
		break;

	case STR_LINK_OPS:
		atomicAddInt(&(strstatp->linkblk.use),c);
		if (strstatp->linkblk.use > strstatp->linkblk.max) {
			strstatp->linkblk.max = strstatp->linkblk.use;
		}
		break;

	case STR_LINK_FAILOPS:
		atomicAddInt(&(strstatp->linkblk.fail),c);
		break;

	case STR_STREVENT_OPS:
		atomicAddInt(&(strstatp->strevent.use),c);
		if (strstatp->strevent.use > strstatp->strevent.max) {
			strstatp->strevent.max = strstatp->strevent.use;
		}
		break;

	case STR_STREVENT_FAILOPS:
		atomicAddInt(&(strstatp->strevent.fail),c);
		break;

	case STR_QBAND_OPS:
		atomicAddInt(&(strstatp->qbinfo.use),c);
		if (strstatp->qbinfo.use > strstatp->qbinfo.max) {
			strstatp->qbinfo.max = strstatp->qbinfo.use;
		}
		break;

	case STR_QBAND_FAILOPS:
		atomicAddInt(&(strstatp->qbinfo.fail),c);
		break;

	default:
		break;
	}
	return;
}

/*
 * Procedure Description:
 * Return index into the buffer size array to use for the requested size buffer
 *
 * Parameters:
 * size => Size in bytes of requested buffer, can be zero, negative or positive
 *
 * Returns:
 *  Index into the buffers size array, and which is also the index into the
 * the buffer list heads store in the per-cpu lf_listvec.
 */
int
str_getbuf_index(int size)
{
	int ix, rtn;

	STR_CALLSTATS(str_getbuf_index_calls);
	if (size <= 0) {
#ifdef DEBUG
		if (size < 0) {
			STR_CALLSTATS(str_getbuf_index_ltzero);
		} else {
			STR_CALLSTATS(str_getbuf_index_zero);
		}
#endif /* DEBUG */
		return STR_BUF64_INDEX;
	}
	rtn = STR_PAGE_INDEX;
	for (ix = STR_BUF64_INDEX; ix <= STR_PAGE_INDEX; ix++) {
		if (str_buffersizes[ix] >= size) {
			rtn = ix;
			break;
		}
	}
#ifdef DEBUG
	if (rtn == STR_BUF64_INDEX) {
		STR_CALLSTATS(str_getbuf_index_64);
		return rtn;
	}
	if (rtn == STR_BUF256_INDEX) {
		STR_CALLSTATS(str_getbuf_index_256);
		return rtn;
	}
	if (rtn == STR_BUF512_INDEX) {
		STR_CALLSTATS(str_getbuf_index_512);
		return rtn;
	}
	if (rtn == STR_BUF2K_INDEX) {
		STR_CALLSTATS(str_getbuf_index_2K);
	} else {
		if (rtn == STR_PAGE_INDEX) {
			STR_CALLSTATS(str_getbuf_index_page);
		}
	}
#endif
	return rtn;
}

/*
 * Return msg/dblk with no buffer attached
 */
/* ARGSUSED */
mblk_t *
allocb_nobuffer(cpuid_t cpu_id)
{
	struct lf_listvec *lf_listvecp;
	struct listvec *listvecp;
	struct lf_elt *page;
	struct msgb *mp;
	struct datab *datap;
	int s_lfvec;

	STR_CALLSTATS(allocb_nobuffer_calls);
	lf_listvecp = STR_LFVEC(cpu_id);
	listvecp = &(lf_listvecp->buffer[STR_MD_INDEX]);
	STR_LISTVECSTATS(listvecp, get_calls, 1);

	s_lfvec = mutex_spinlock(&lf_listvecp->lock);

	if ((mp = (struct msgb *)(listvecp->head))) {

		if ((listvecp->head = ((struct lf_elt *)mp)->next) == NULL) {
			listvecp->tail = NULL;
		}
		listvecp->nodecnt--;
		STR_LISTVECSTATS(listvecp, get_local, 1);

		/*
		 * msgb/dblk pair allocated.
		 * Initialize fields in order of structure element declaration
		 * in the off chance that helps the compiler's code generation
		 *
		 * NOTE: Must have mutex lock held when we arrive here
		 */
		mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

	} else { /* failed to get combined mblk/dblk as list is empty */

		mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

		/* try an allocate page of memory and tile it */
		if (page = lf_getpage_tiled(S_DONTWAIT, STR_MD_SIZE,
				VM_DIRECT|VM_STALE, cpu_id)) {
			/*
			 * add rest of nodes in page to list and
			 * return first one
			 */
			STR_LISTVECSTATS(listvecp, get_addpage, 1);
			mp = (struct msgb *)lf_addpage(lf_listvecp,
				STR_MD_INDEX, page);
		} else {
			/* failed to get page for msg/dblk */
			str_stats_ops(cpu_id, STR_MD_INDEX,
				STR_BUFFER_FAILOPS, 1);
			STR_LISTVECSTATS(listvecp, get_fail, 1);
			STR_CALLSTATS(allocb_nobuffer_fail);
			return ((struct msgb *)0);
		}
	}
	mp->b_next = mp->b_prev = mp->b_cont = NULL;
	mp->b_rptr = mp->b_wptr = NULL;
	mp->b_datap = datap =
		(struct datab *)((char *)mp + sizeof(struct mbinfo));
	mp->b_band = 0;
	mp->b_flag = 0;
	mp->b_cpuid = cpu_id;
	mp->b_mopsflag = STR_MOPS_ALLOCMD;
	mp->b_mbuf_refct = 0;

	SAVE_ALLOCB_ADDRESS(mp, __return_address);
	SAVE_DUPB_ADDRESS(mp, NULL);
	SAVE_FREEB_ADDRESS(mp, NULL);

	SAVE_PUTQ_QADDRESS(mp, NULL);
	SAVE_PUTNEXT_Q_ADDRESS(mp, NULL);
	SAVE_PUTNEXT_QIPUTP_ADDRESS(mp, NULL);
	/*
	 * Initialize data block descriptor
	 */
	datap->db_frtnp = NULL;
	datap->db_base = datap->db_lim = NULL;
	datap->db_ref = 1;
	datap->db_size = 0;
	datap->db_type = M_DATA;
	datap->db_index = 0;

	/* save cpu_id where msg/dblk and buffer allocated */
	datap->db_cpuid = cpu_id;
	datap->db_buf_cpuid = 0;

	/* now set the owning msgb address */
	datap->db_msgaddr = mp;
	datap->db_frtn.free_func = NULL;
	datap->db_frtn.free_arg = NULL;

	str_stats_ops(cpu_id, STR_MD_INDEX, STR_BUFFER_OPS, 1);

	STR_MEM_TRACE(datap, ALLOCEDBY, (inst_t *)__return_address, 0);
	STR_CALLSTATS(allocb_nobuffer_ok);
	INSERT_MSG_INUSE((struct mbinfo *)mp);

	return mp;
}

/*
 * Free the large Streams buffer allocated from the kernel heap
 */
void
allocb_freebigbuf(char *buf, int size)
{
	STR_CALLSTATS(allocb_freebigbuf_calls);
	kmem_free((void *)buf, size);
	return;
}

/*
 * Allocate and return a msg/dblk header along with a buffer of 'size'
 * bytes in length.
 * 'pri' is no longer used, but is retained for compatibility.
 */
/* ARGSUSED */
mblk_t *
allocb(int size, uint pri)
{
	struct lf_listvec *lf_listvecp;
	struct listvec *listvecp, *buf_listvecp;
	struct lf_elt *page;
	struct msgb *mp;
	struct datab *datap;
	unsigned char *buf;
	int index, newsize, s_lfvec;
	struct freeext_rtn freeext, *freeextp;
	cpuid_t cpu_id = cpuid();

	STR_CALLSTATS(allocb_calls);
	if (size == -1) { /* handle rare case where no buffer is desired */
		goto allocb_nobuf;
	}
	if (size > STR_MAXBSIZE) { /* handle case buffer > page size */
		goto allocb_hugebuf;
	}
	index = str_getbuf_index(size);
	newsize = str_buffersizes[index];

	ASSERT(index >= STR_BUF64_INDEX);
	ASSERT(newsize >= size);

	lf_listvecp = STR_LFVEC(cpu_id);
	listvecp = &(lf_listvecp->buffer[STR_MD_INDEX]);
       	buf_listvecp = &(lf_listvecp->buffer[index]);

	STR_LISTVECSTATS(listvecp, get_calls, 1);
	s_lfvec = mutex_spinlock(&lf_listvecp->lock);

	if ((mp = (struct msgb *)(listvecp->head))) {

		if ((listvecp->head = ((struct lf_elt *)mp)->next) == NULL) {
			listvecp->tail = NULL;
		}
		listvecp->nodecnt--;
		STR_LISTVECSTATS(listvecp, get_local, 1);

		/*
		 * msgb/dblk pair allocated so see if we can get a buffer.
		 * Initialize fields in order of structure element declaration
		 * in the off chance that helps the compiler's code generation
		 * NOTE: Must have mutex lock held when we arrive here
		 */
allocb_getbuf:
		STR_LISTVECSTATS(buf_listvecp, get_calls, 1);

		if (buf = (unsigned char *)(buf_listvecp->head)) {

			STR_LISTVECSTATS(buf_listvecp, get_local, 1);
			/*
			 * Update list head/tail pointers after getting buffer
			 */
			if ((buf_listvecp->head = ((struct lf_elt *)buf)->next)
				== NULL) {
				buf_listvecp->tail = NULL;
			}
			buf_listvecp->nodecnt--;
			mutex_spinunlock(&lf_listvecp->lock, s_lfvec);
			/*
			 * Initialize message block descriptor
			 */
allocb_fill:		mp->b_next = mp->b_prev = mp->b_cont = NULL;
			mp->b_rptr = mp->b_wptr = buf;
			mp->b_datap = datap =
			 (struct datab *)((char *)mp + sizeof(struct mbinfo));

			mp->b_band = 0;
			mp->b_flag = 0;
			mp->b_cpuid = cpu_id;
			mp->b_mopsflag = STR_MOPS_ALLOCMD;
			mp->b_mbuf_refct = 0;

			SAVE_ALLOCB_ADDRESS(mp, __return_address);
			SAVE_DUPB_ADDRESS(mp, NULL);
			SAVE_FREEB_ADDRESS(mp, NULL);

			SAVE_PUTQ_QADDRESS(mp, NULL);
			SAVE_PUTNEXT_Q_ADDRESS(mp, NULL);
			SAVE_PUTNEXT_QIPUTP_ADDRESS(mp, NULL);
			/*
			 * Initialize data block descriptor
			 */
			datap->db_frtnp = NULL;
			datap->db_base = buf;
			datap->db_lim = buf + newsize;
			datap->db_ref = 1;
			datap->db_size = newsize;
			datap->db_type = M_DATA;
			datap->db_index = (unsigned char)(index & 0xff);

			/* save cpu_id where msg/dblk and buffer allocated */
			datap->db_cpuid = datap->db_buf_cpuid = cpu_id;

			/* now set the owning msgb address */
			datap->db_msgaddr = mp;

			datap->db_frtn.free_func = NULL;
			datap->db_frtn.free_arg = NULL;

			/* optimize updating md and buffer statistics */
			str_stats_ops(cpu_id, index, STR_MD_BUF_OPS, 1);

			STR_MEM_TRACE(datap, ALLOCEDBY,
				(inst_t *)__return_address, 0);
			STR_CALLSTATS(allocb_ok);
			INSERT_MSG_INUSE((struct mbinfo *)mp);

			return mp;
		}
		/*
		 * failed to get requested size buffer
		 */
		mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

		if (page = lf_getpage_tiled(S_DONTWAIT, newsize,
			VM_DIRECT|VM_STALE, cpu_id)) { /* got page */

			STR_LISTVECSTATS(buf_listvecp, get_addpage, 1);

			if (newsize < NBPP) {
				/*
				 * Node size less than an entire page so add
				 * the page of linked nodes safely to the get
				 * list and return the first node.
				 */
				buf = (unsigned char *)lf_addpage(lf_listvecp,
					(int)index, page);
			} else { /* page size buffer requested */
				buf = (unsigned char *)page;
			}
			goto allocb_fill;
		}
		/*
		 * failed to get buffer so return allocated msgb/datab block
		 */
		lf_free(lf_listvecp, STR_MD_INDEX, (struct lf_elt *)mp,
			cpu_id);

		str_stats_ops(cpu_id, index, STR_BUFFER_FAILOPS, 1);

		STR_LISTVECSTATS(buf_listvecp, get_fail, 1);
		STR_CALLSTATS(allocb_buf_fail);

	} else { /* failed to get combined mblk/dblk */

		mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

		/* try an allocate page of memory and tile it */
		if (page = lf_getpage_tiled(S_DONTWAIT, STR_MD_SIZE,
				VM_DIRECT|VM_STALE, cpu_id)) {
			/*
			 * add rest of nodes in page to list and return first
			 */
			STR_LISTVECSTATS(listvecp, get_addpage, 1);
			mp = (struct msgb *)lf_addpage(lf_listvecp,
				STR_MD_INDEX, page);

			/* re-acquire lock on per-cpu structure */
			s_lfvec = mutex_spinlock(&lf_listvecp->lock);
			goto allocb_getbuf;
		}
		/* failed to get msg/dblk combined block */
		str_stats_ops(cpu_id, STR_MD_INDEX, STR_BUFFER_FAILOPS, 1);

		STR_LISTVECSTATS(listvecp, get_fail, 1);
		STR_CALLSTATS(allocb_md_fail);
	}
	return ((struct msgb *)0);
/* 
 * Allocate a msg/dblk without a buffer attached
 */
allocb_nobuf:
#ifdef DEBUG
	mp = allocb_nobuffer(cpu_id);
	if (mp) {
		STR_CALLSTATS(allocb_nobuf_ok);
	} else { /* failed to get huge buffer */
		STR_CALLSTATS(allocb_nobuf_fail);
	}
	return mp;
#else
	return (allocb_nobuffer(cpu_id));
#endif /* DEBUG */

/* 
 * Allocate a msg/dblk with a buffer attached whose size exceeds the maximum
 * supported. This is for those cases where size > STR_MAXBSIZE.
 */
allocb_hugebuf:
	if (buf = (unsigned char *)kmem_alloc(size, KM_NOSLEEP)) {

		/* got a buffer without sleeping */
		freeext.free_func = (void (*)())allocb_freebigbuf;
		freeext.free_arg = (char *)buf;
		freeext.size_arg = size;

		if (mp = esballoc(buf, size, pri, (frtn_t *)(&freeext))) {
			/*
			 * Now store/update the real function and arg to the
			 * extended alloc procedure since brain-dead esballoc
			 * only updates the pointer to the function storage
			 * in the dblk and NOT its content.
			 * NOTE: The extended function does NOT have
			 * storage to hold the integer size value that is
			 * passed in by the freeb procedure when it detects
			 * this special case.
			 */
			freeextp =
				(struct freeext_rtn *)(&mp->b_datap->db_frtn);
			freeextp->free_func = (void (*)())allocb_freebigbuf;
			freeextp->free_arg = (char *)buf;
			mp->b_datap->db_frtn_extp = freeextp;

		} else { /* failed so return buffer */
			kmem_free((void *)buf, size);
		}
	} else { /* failed to get huge buffer */
		mp = NULL;
	}
#ifdef DEBUG
	if (mp) {
		STR_CALLSTATS(allocb_bigbuf_ok);
	} else { /* failed to get huge buffer */
		STR_CALLSTATS(allocb_bigbuf_fail);
	}
#endif /* DEBUG */
	return mp;
} 

/* 
 * Allocate a class zero data block and attach an externally-supplied
 * data buffer pointed to by base to it.
 */
/* ARGSUSED */
mblk_t *
esballoc(unsigned char *base, int size, int pri, frtn_t *fr_rtn)
{
	register struct msgb *mp;
	cpuid_t cpu_id = cpuid();

	STR_CALLSTATS(esballoc_calls);

	if (!base || !fr_rtn) {
		STR_CALLSTATS(esballoc_fail_null);
		return NULL;
	}
	if (mp = allocb_nobuffer(cpu_id)) {

		ASSERT(mp->b_datap->db_base == NULL);
		ASSERT(mp->b_datap->db_base == mp->b_datap->db_lim);
		ASSERT(mp->b_rptr == NULL);
		ASSERT(mp->b_rptr == mp->b_wptr);

		mp->b_rptr = mp->b_wptr = base;
		mp->b_mopsflag = STR_MOPS_ALLOCMD_ESB;

		mp->b_datap->db_base = base;
		mp->b_datap->db_lim = base + size;
		mp->b_datap->db_size = size;
		mp->b_datap->db_frtnp = fr_rtn;
		/*
		 * db_buf_cpuid probably should be non-zero for -1 case when
		 * and external buffer supplied for NUMA aware buffer
		 * allocator for the external buffer manager. For now
		 * it is zero'ed by allocb when -1 is passed in.
		 */
		STR_MEM_TRACE((mp->b_datap), ALLOCEDBY,
			(inst_t *)__return_address,0);
		STR_CALLSTATS(esballoc_ok);
	} else {
		STR_CALLSTATS(esballoc_fail_allocb);
	}
	return mp;
}

/*
 * Call function 'func' with 'arg' when a class zero block can 
 * be allocated with priority 'pri'.
 */
/* ARGSUSED */
toid_t
esbbcall(int pri, void (*func)(), long arg)
{
	register int s;
	struct strevent *sep;

	/*
	 * allocate new stream event to add to linked list 
	 */
	STR_CALLSTATS(esbbcall_calls);
	if (!(sep = sealloc(SE_NOSLP))) {
		cmn_err_tag(163,CE_WARN,"esbbcall: could not allocate stream event\n");
		STR_CALLSTATS(esbbcall_fail_sealloc);
		return(0);
	}
	sep->se_next = NULL;
	sep->se_func = func;
	sep->se_arg = arg;
	sep->se_size = -1;
	/*
	 * add newly allocated stream event to existing
	 * linked list of events.
	 */
	LOCK_STR_RESOURCE(s);
	if (strbcalls.bc_head == NULL) {
		strbcalls.bc_head = strbcalls.bc_tail = sep;
	} else {
		strbcalls.bc_tail->se_next = sep;
		strbcalls.bc_tail = sep;
	}
	UNLOCK_STR_RESOURCE(s);

	strbcwait = 1;	/* so freeb will know to call setqsched() */
	STR_CALLSTATS(esbbcall_ok);
	return((toid_t)(__psint_t)sep);
}

/*
 * test if block of given size can be allocated with a request of
 * the given priority.
 * 'pri' is no longer used, but is retained for compatibility.
 */
/* ARGSUSED */
int
testb(int size, uint pri)
{
	STR_CALLSTATS(testb_calls);
	return(size <= kmem_avail());
}

/*
 * Call function 'func' with argument 'arg' when there is a reasonably
 * good chance that a block of size 'size' can be allocated.
 * 'pri' is no longer used, but is retained for compatibility.
 */
/* ARGSUSED */
toid_t
bufcall(uint size, int pri, void (*func)(), long arg)
{
	struct strevent *sep;
	register int s;

	STR_CALLSTATS(bufcall_calls);

	if (!(sep = sealloc(SE_NOSLP))) {
		cmn_err_tag(164,CE_WARN,"bufcall: could not allocate stream event\n");
		STR_CALLSTATS(bufcall_fail_sealloc);
		return 0;
	}
	sep->se_next = NULL;
	sep->se_func = func;
	sep->se_arg = arg;
	sep->se_size = size;

	/*
	 * Stash a pointer to a pointer to the currently held monitor
	 * so that we can reacquire the correct monitor before we call se_func.
	 * Double-indirection is necessary because monitor might get swapped.
	 */
	sep->se_monpp = curthreadp->k_activemonpp;

	/*
	 * add newly allocated stream event to existing linked list of events.
	 */
	LOCK_STR_RESOURCE(s);
        if (strbcalls.bc_head == NULL) {
		strbcalls.bc_head = strbcalls.bc_tail = sep;
	} else {
                strbcalls.bc_tail->se_next = sep;
		strbcalls.bc_tail = sep;
	}
	UNLOCK_STR_RESOURCE(s);

	strbcwait = 1;	/* so freeb will know to call setqsched() */
	STR_CALLSTATS(bufcall_ok);
	return((toid_t)(__psint_t)sep);
}

/*
 * Cancel a bufcall request.
 */
void
unbufcall(toid_t id)
{
	register struct strevent *sep, *psep;
	register int s;

	psep = NULL;

	LOCK_STR_RESOURCE(s);
	for (sep = strbcalls.bc_head; sep; sep = sep->se_next) {
		if (id == (toid_t)(__psint_t)sep)
			break;
		psep = sep;
	}
	if (sep) {
		if (psep) {
			psep->se_next = sep->se_next;
		} else {
			strbcalls.bc_head = sep->se_next;
		}
		if (sep == strbcalls.bc_tail)
			strbcalls.bc_tail = psep;
		sefree(sep);
	}
	UNLOCK_STR_RESOURCE(s);
	return;
}

/*
 * Copy data from message block to newly allocated message block and
 * data block.  The copy is rounded out to full word boundaries so that
 * the (usually) more efficient word copy can be done.
 * Returns new message block pointer, or NULL if error.
 */
mblk_t *
copyb(mblk_t *bp)
{
	register mblk_t *nbp;
	register dblk_t *dp, *ndp;
	caddr_t base;

	ASSERT(bp);
	ASSERT(bp->b_wptr >= bp->b_rptr);

	dp = bp->b_datap;
	if (!(nbp = allocb(dp->db_lim - dp->db_base, BPRI_MED))) { /* failed */
		return NULL;
	}
	nbp->b_flag = bp->b_flag;
	nbp->b_band = bp->b_band;
	ndp = nbp->b_datap;	
	ndp->db_type = dp->db_type;

	nbp->b_rptr = ndp->db_base + (bp->b_rptr - dp->db_base);
	nbp->b_wptr = ndp->db_base + (bp->b_wptr - dp->db_base);

	base = straln(nbp->b_rptr);
	strbcpy(straln(bp->b_rptr),
		base,
		straln(nbp->b_wptr + (sizeof(__psint_t)-1)) - base);
	return nbp;
}

/*
 * Trim bytes from message
 *  len > 0, trim from head
 *  len < 0, trim from tail
 * Returns 1 on success, 0 on failure.
 */
int
adjmsg(mblk_t *mp, int len)
{
	register mblk_t *bp, *endbp;
	int fromhead, n;

	ASSERT(mp != NULL);

	STR_CALLSTATS(adjmsg_calls);
	fromhead = 1;
	if (len < 0) {
		fromhead = 0;
		len = -len;
	}
	if (xmsgsize(mp, fromhead) < len) { /* failed */
		STR_CALLSTATS(adjmsg_fail_xmsgsize);
		return 0;
	}
	if (fromhead) {
		STR_CALLSTATS(adjmsg_fromhead);
		bp = mp;
		while (len) {
			n = min(bp->b_wptr - bp->b_rptr, len);
			bp->b_rptr += n;
			len -= n;
			bp = bp->b_cont;
		}
	} else {
		STR_CALLSTATS(adjmsg_fromtail);
		endbp = NULL;
		while (len) {
			for (bp = mp; bp->b_cont != endbp; bp = bp->b_cont)
				;
			n = min(bp->b_wptr - bp->b_rptr, len);
			bp->b_wptr -= n;
			len -= n;
			endbp = bp;
		}
	}
	return 1;
}

/*
 * Copy data from message to newly allocated message using new
 * data blocks.  Returns a pointer to the new message, or NULL if error.
 */
mblk_t *
copymsg(mblk_t *bp)
{
	register mblk_t *head, *nbp;

	STR_CALLSTATS(copymsg_calls);
	if (!bp) {
		STR_CALLSTATS(copymsg_fail_null);
		return NULL;
	}
	if (!(nbp = head = copyb(bp))) {
		STR_CALLSTATS(copymsg_fail_copyb);
		return NULL;
	}
	while (bp->b_cont) {
		if (!(nbp->b_cont = copyb(bp->b_cont))) {
			freemsg(head);
			STR_CALLSTATS(copymsg_fail_copyb);
			return NULL;
		}
		nbp = nbp->b_cont;
		bp = bp->b_cont;
	}
	STR_CALLSTATS(copymsg_ok);
	return head;
}

/*
 * Duplicate a message block
 *
 * Allocate a message block and assign proper values to it
 * (read and write pointers) and link it to existing data block.
 * Increment reference count of data block.
 */
mblk_t *
dupb(mblk_t *mp)
{
	register struct lf_listvec *lf_listvecp;
	register struct msgb *newmp;
	cpuid_t cpu_id = cpuid();

	ASSERT(mp);
	ASSERT(mp->b_datap->db_ref > 0);
	STR_CALLSTATS(dupb_calls);

	/* allocate msg block HOPEFULLY on same cpu as the one we're dup'ing */
	lf_listvecp = STR_LFVEC(cpu_id);

	newmp = (struct msgb *)lf_get(lf_listvecp, STR_MSG_INDEX,
			cpu_id, S_DONTWAIT);

	if (newmp == NULL) { /* ran out of memory and couldn't wait */
		str_stats_ops(cpu_id, STR_MSG_INDEX, STR_BUFFER_FAILOPS, 1);
		STR_CALLSTATS(dupb_fail_nomsg);
		return NULL;
	}
	str_stats_ops(cpu_id, STR_MSG_INDEX, STR_BUFFER_OPS, 1);

	INSERT_MSG_INUSE((struct mbinfo *)newmp);
	SAVE_ALLOCB_ADDRESS(newmp, __return_address);
	SAVE_DUPB_ADDRESS(mp, __return_address);
	
	/* just for debugging now */
	SAVE_DUPB_ADDRESS(newmp, __return_address);

	newmp->b_datap = mp->b_datap;
	atomicAddInt(&(mp->b_datap->db_ref), 1);

	newmp->b_next = newmp->b_prev = newmp->b_cont = NULL;
	newmp->b_rptr = mp->b_rptr;
	newmp->b_wptr = mp->b_wptr;
	newmp->b_flag = mp->b_flag;
	newmp->b_band = mp->b_band;
	newmp->b_cpuid = cpu_id;
	newmp->b_mopsflag = STR_MOPS_ALLOCMSG;
	newmp->b_mbuf_refct = mp->b_mbuf_refct;

	ASSERT(newmp->b_rptr >= mp->b_datap->db_base);
	ASSERT(newmp->b_rptr <= mp->b_datap->db_lim);
	ASSERT(newmp->b_wptr >= mp->b_datap->db_base);
	ASSERT(newmp->b_wptr <= mp->b_datap->db_lim);

	STR_MEM_TRACE(newmp, DUPEDBY, (inst_t *)__return_address, mp);
	STR_MEM_TRACE(mp, DUPEDBY, (inst_t *)__return_address, newmp);

	STR_CALLSTATS(dupb_ok);
	return newmp;
} 

/*
 * Duplicate message block by block (uses dupb)
 * Returns non-NULL address of duplicate message ONLY if entire msg was dup'd.
 */
mblk_t *
dupmsg(mblk_t *mp)
{
	register mblk_t *head, *newmp;

	STR_CALLSTATS(dupmsg_calls);
	if (!mp) {
		STR_CALLSTATS(dupmsg_fail_null);
		return NULL;
	}
	head = newmp = dupb(mp);
	if (!head) {
		STR_CALLSTATS(dupmsg_fail_dupb);
		return NULL;
	}
	while (mp->b_cont) {

		newmp->b_cont = dupb(mp->b_cont);
		STR_CALLSTATS(dupmsg_dupb);

		if (!(newmp->b_cont)) { /* failed so free what was dup'ed */
			freemsg(head);
			STR_CALLSTATS(dupmsg_fail_dupb);
			return NULL;
		}
		newmp = newmp->b_cont;
		mp = mp->b_cont;
	}
	STR_CALLSTATS(dupmsg_ok);
	return head;
}

#ifdef DEBUG
/*
 * Free a message block procedure
 */
void
freeb(mblk_t *mp)
{
	freeb_debug(mp, __return_address);
	return;
}

/*
 * Free a message block procedure
 */
void
freeb_debug(mblk_t *mp, inst_t *ra)
#else

void
freeb(mblk_t *mp)
#endif /* DEBUG */
{
	register struct lf_listvec *lf_listvecp, *buflf_listvecp;
	register struct datab *datap;
	mblk_t *mp_own;
	int not_ours;
	cpuid_t mp_cpuid, buf_cpuid, mp_own_cpuid, alloc_cpuid;

	STR_CALLSTATS(freeb_calls);
	ASSERT(mp);

	datap = mp->b_datap;
	ASSERT(datap != 0);
	ASSERT(datap->db_ref > 0);

	/* check if this datablk is associated with this message block */
	not_ours = (datap->db_msgaddr != mp) ? 1 : 0;

	/*
	 * compute list head address on cpu where msg blk was allocated
	 */
	mp_cpuid = mp->b_cpuid;
	lf_listvecp = STR_LFVEC(mp_cpuid);

	/*
	 * get address of dblk list head for the cpu where it was
	 * allocated and then claim that lock.
	 *
	 * The test of the dblk ownership above avoids the case when the
	 * thread is pre-empted IMMEDIATELY after the atomic decrement
	 * operation of the refcnt below and some other thread proceeds to
	 * deallocate the dblk node. When the first thread resumes, the node
	 * is now trash since it was deallocated by the second thread.
	 */
	if (atomicAddInt(&(datap->db_ref), -1) > 0) { /* other ref's remain */
		/*
		 * We can NOT ASSERT(datap->db_ref > 0) is true here since we
		 * don't have a lock protecting the decrement and other threads
		 * may be doing free's on this dup'ed message.
		 */
		if (not_ours) { /* NOT our datablk to deallocate */
			/*
			 * The owning msg to this dblk is not us,
			 * so free msg 'mp' only
			 */
#ifdef DEBUG
			SAVE_FREEB_ADDRESS(mp, ra);
#else
			SAVE_FREEB_ADDRESS(mp, __return_address);
#endif /* DEBUG */
			mp->b_mopsflag |= STR_MOPS_FREEMSG;

			DELETE_MSG_INUSE((struct mbinfo *)mp);

			lf_free(lf_listvecp, STR_MSG_INDEX,
				(struct lf_elt *)mp, mp_cpuid);

			str_stats_ops(mp_cpuid, STR_MSG_INDEX, STR_BUFFER_OPS,
				-1);
			STR_CALLSTATS(freeb_decrefcnt_notourmsg);

		} else {
			STR_CALLSTATS(freeb_decrefcnt_ourmsg);
		}
		return;
	}

	/*
	 * Last reference on dblk, so release it's associated buffer
	 */
	STR_CALLSTATS(freeb_refcnt_zero);
	ASSERT(datap->db_ref == 0);

	STR_MEM_TRACE(datap, FREEDBY, ra, 0);

	if (datap->db_frtnp == NULL) { /* no extended function */

		ASSERT(((char *)datap->db_lim - (char *)datap->db_base)
			== datap->db_size);
		ASSERT(datap->db_index >= STR_BUF64_INDEX);

		buf_cpuid = datap->db_buf_cpuid;
		buflf_listvecp = STR_LFVEC(buf_cpuid);

		datap->db_msgaddr->b_mopsflag |= STR_MOPS_FREEMD;

		lf_free(buflf_listvecp, (int)(datap->db_index),
			(struct lf_elt *)(datap->db_base), buf_cpuid);

		str_stats_ops(buf_cpuid, (int)(datap->db_index),
			STR_BUFFER_OPS, -1);
		STR_CALLSTATS(freeb_frtnp_null);
	} else {
		/*
		 * NOTE: All free functions are now required to be MP-safe
		 * so that must be handled in al Streams device drivers
		 * and particularly with respect to kernel threads.
		 */
		if (datap->db_frtnp->free_func) {

			if (datap->db_frtn_extp->free_func !=
					allocb_freebigbuf) {

				(*datap->db_frtnp->free_func)
					(datap->db_frtnp->free_arg);

				datap->db_msgaddr->b_mopsflag |=
					STR_MOPS_MBUF_TO_MBLK_FREE;
			} else {
				(*datap->db_frtn_extp->free_func)
					(datap->db_frtn_extp->free_arg,
					(int)datap->db_size);
			}
			datap->db_msgaddr->b_mopsflag |= STR_MOPS_FREEMD_ESB;

			STR_CALLSTATS(freeb_extfun);
		} else {
			STR_CALLSTATS(freeb_extfun_null);
		}
	}
	if (datap->db_msgaddr != mp) { /* NOT owner */
		/*
		 * We're not the owning msg to this dblk, ie contiguous msg
		 * header for this mblk/dblk pair SO first release mblk 'mp'
		 * then 'db_msgaddr' field to get owning mblk's address
		 * for the mblk/dblk pair.
		 */
#ifdef DEBUG
		SAVE_FREEB_ADDRESS(mp, ra);
#else
		SAVE_FREEB_ADDRESS(mp, __return_address);
#endif /* DEBUG */

		mp->b_mopsflag |= STR_MOPS_FREEMSG;
		DELETE_MSG_INUSE((struct mbinfo *)mp);

		lf_free(lf_listvecp, STR_MSG_INDEX, (struct lf_elt *)mp,
			mp_cpuid);

		str_stats_ops(mp_cpuid, STR_MSG_INDEX, STR_BUFFER_OPS, -1);
		STR_CALLSTATS(freeb_notourmsg);
	} else { /* dblk belongs to this msg hdr; So owner's last free'er */
		STR_CALLSTATS(freeb_ourmsg);
	}

	/* Now deallocate this msg/dblk pair together */
	mp_own = datap->db_msgaddr;
	mp_own_cpuid = datap->db_msgaddr->b_cpuid;

	if (mp_own_cpuid != mp_cpuid) { /* msg/dblk on another cpu */
		lf_listvecp = STR_LFVEC(mp_own_cpuid);
		alloc_cpuid = mp_own_cpuid;
	} else {
		alloc_cpuid = mp_cpuid;
	}

#ifdef DEBUG
	SAVE_FREEB_ADDRESS(mp_own, ra);
#else
	SAVE_FREEB_ADDRESS(mp_own, __return_address);
#endif /* DEBUG */
	DELETE_MSG_INUSE((struct mbinfo *)mp_own);

	lf_free(lf_listvecp, STR_MD_INDEX,
		(struct lf_elt *)(mp_own), alloc_cpuid);

	str_stats_ops(mp_own_cpuid, STR_MD_INDEX, STR_BUFFER_OPS, -1);

	if (strbcwait && !strbcflag) {
		/*
		 * call qsched when we release a message/dblk and we have
		 * can outstanding buffcall request, waiting for memory
		 */
		STR_CALLSTATS(freeb_strbcflag);
		strbcflag = 1;
		setqsched();
	}
	return;
}

/*
 * Free all message blocks in a message using freeb() OR the special
 * debug version that allows you to pass in the real return address
 * to note the callers of freemsg.
 * The message pointer can also be NULL.
 */
void
freemsg(mblk_t *mp)
{
	mblk_t *tmp;
#ifdef DEBUG
	int mblk_cnt = 0;
#endif /* DEBUG */

	STR_CALLSTATS(freemsg_calls);
	if (!mp) { /* null */
		STR_CALLSTATS(freemsg_null);
		return;
	}
	while (mp) {
		tmp = mp->b_cont;
#ifdef DEBUG
		freeb_debug(mp, __return_address);
		mblk_cnt++;
#else
		freeb(mp);
#endif /* DEBUG */
		mp = tmp;
	}

#ifdef DEBUG
	STR_CALLSTATS_INC(freemsg_freeb, mblk_cnt);
	STR_CALLSTATS(freemsg_ok);
#endif /* DEBUG */

	return;
}

/* 
 * link a message block to tail of message
 */
void
linkb(mblk_t *mp, mblk_t *newmp)
{
	ASSERT(mp && newmp);

	STR_CALLSTATS(linkb_calls);
	for (; mp->b_cont; mp = mp->b_cont) {
		;
	}
	ASSERT(mp->b_next == 0);
	ASSERT(mp->b_prev == 0);
	mp->b_cont = newmp;
	return;
}

/*
 * unlink a message block from head of message return pointer to new message.
 * NULL if message becomes empty.
 */
mblk_t *
unlinkb(mblk_t *mp)
{
	register mblk_t *mp1;

	ASSERT(mp);
	STR_CALLSTATS(unlinkb_calls);
	mp1 = mp->b_cont;
	mp->b_cont = NULL;
	return mp1;
}

/* 
 * remove a message block "bp" from message "mp"
 *
 * Return pointer to new message or NULL if no message remains.
 * Return -1 if bp is not found in message.
 */
mblk_t *
rmvb(mblk_t *mp, mblk_t *bp)
{
	register mblk_t *tmp, *lastp = NULL;

	STR_CALLSTATS(rmvb_calls);
	ASSERT(mp && bp);

	for (tmp = mp; tmp; tmp = tmp->b_cont) {
		if (tmp == bp) {
			if (lastp) {
				lastp->b_cont = tmp->b_cont;
			} else {
				mp = tmp->b_cont;
			}
			tmp->b_cont = NULL;
			return mp;
		}
		lastp = tmp;
	}
	return((mblk_t *)-1L);
}

/*
 * Initialize the giveback timer
 * This is called from the tbd table in the kernels main procedure AFTER
 * memory has been ihitialized and the giveback timer thread can be created.
 */
void
strinit_giveback(void)
{
	/*
	 * NOTE: Timeout is set initially for 120 seconds and subsequently
	 * gets reset to scan the free page lists, one cpu every 60 seconds
	 * in a round-robin fashion.
	 */
	if (strgiveback_token == 0) {
		extern int rmonqd_pri;
		strgiveback_token = timeout_pri(
			(strtimeoutfunc_t)strgiveback,
			0, STRGIVEBACK_INIT, rmonqd_pri+1);
	}
	return;
}

/*
 * The actual workhourse giveback timer procedures which determines
 * which free pages used by Streams can be returned to the kernel.
 */
void
strgiveback(void)
{
	struct lf_listvec *lf_listvecp;
	struct listvec *listvecp;
	struct lf_elt *newlisthead, *next_page, *page;
	int i, remove_nodecnt, s_lfvec;

	if (pdaindr[strgiveback_cpuid].CpuId == -1) {
		/* skip if this cpu is not valid */
		goto strgiveback_quit;
	}
	if (!(pdaindr[strgiveback_cpuid].pda->p_flags & PDAF_ENABLED)) {
		/* skip if this cpu is not enabled */
		goto strgiveback_quit;
	}
	lf_listvecp = STR_LFVEC(strgiveback_cpuid);
	if (lf_listvecp == NULL) {
		/* fail if vector address NULL */
		goto strgiveback_quit;
	}

	listvecp = &(lf_listvecp->buffer[STR_PAGE_INDEX]);
	newlisthead = NULL;
	remove_nodecnt = 0;

	/* claim local cpu list lock to check counts */
	s_lfvec = mutex_spinlock(&lf_listvecp->lock);

	/* compute average node count over last interval */
	listvecp->avg_nodecnt = (listvecp->avg_nodecnt+listvecp->nodecnt) >> 1;

	if ((listvecp->avg_nodecnt > listvecp->threshold) ||
	    (str_page_cur > str_min_pages)) {
		/*
		 * Release 1/4 of the average node count of the free page list
		 * on this cpu during this interval, assuming we're over the
		 * minimum page count OR exceed the threshold for the page list
		 */
		remove_nodecnt = min(listvecp->avg_nodecnt,
				     (str_page_cur - str_min_pages));

		if (remove_nodecnt <= 0) { /* check for exit case */
			/*
			 * Since remove ct was negative we know we aren't over
			 * minimum page count, so return a percentage over the
			 * free page threshold count for this cpu.
			 */
			if (listvecp->avg_nodecnt > listvecp->threshold) {

				remove_nodecnt = listvecp->avg_nodecnt;
				if (remove_nodecnt > 0) { /* continue */
					goto strgiveback_cont;
				}
			}
			mutex_spinunlock(&lf_listvecp->lock, s_lfvec);
			goto strgiveback_quit;
		}
strgiveback_cont:
		if (remove_nodecnt > 3) {
			remove_nodecnt = remove_nodecnt >> 2;
		}

		for (i=0; i < remove_nodecnt; i++) {

			if (page = listvecp->head) {

				listvecp->head = page->next;
				if (listvecp->head == NULL) {
					listvecp->tail = NULL;
				}
				listvecp->nodecnt--;

				if (newlisthead == NULL) {
					page->next = NULL;
				} else {
					page->next = newlisthead;
				}
				newlisthead = page;
			} else {
				break;
			}
		}
	}
	/* release local cpu list lock */
	mutex_spinunlock(&lf_listvecp->lock, s_lfvec);

	page = newlisthead;
	while (page) {
		next_page = page->next;

		/* Update global current page count and num pages released */
		atomicAddInt(&str_page_cur, -1);
		atomicAddInt(&str_page_rel, 1);

		/* return page to kernel */
		kvpfree(page, 1);

		/* get next page address */
		page = next_page;
	}
	while (cvsema(&str_page_sema)) ;

	/* compute next time to allow page release */
	page_released_lbolt = lbolt;
	page_keep_lbolt = page_released_lbolt + PAGE_KEEP_LBOLT;

strgiveback_quit:

#ifdef DEBUG
	strgiveback_lastrel = remove_nodecnt;
	strgiveback_lbolt = lbolt;

	atomicAddInt(&strgiveback_totrel, remove_nodecnt);
	atomicAddInt(&strgiveback_calls, 1);
#endif /* DEBUG */

	strgiveback_last_cpuid = strgiveback_cpuid;
	(void)atomicIncWithWrap(&strgiveback_cpuid, maxcpus);

	strgiveback_token = itimeout(strgiveback, 0, STRGIVEBACK_TO, plbase);
	ASSERT(strgiveback_token != 0);
	return;
}

/*
 * Return size of message of block type (bp->b_datap->db_type)
 * If fromhead is non-zero, then start at beginning of message.
 * If fromhead is zero, then start at end of message.
 */
int
xmsgsize(mblk_t *mp, int fromhead)
{
	register mblk_t *bp;
	register unsigned char type;
	register int count;
	mblk_t *endbp;
	
	if (fromhead) {
		bp = mp;
		type = bp->b_datap->db_type;
		count = 0;
		for (; bp; bp = bp->b_cont) {
			if (type != bp->b_datap->db_type)
				break;
			ASSERT(bp->b_wptr >= bp->b_rptr);
			count += bp->b_wptr - bp->b_rptr;
		}
	} else {
		for (bp = mp; bp->b_cont; bp = bp->b_cont)
			;
		type = bp->b_datap->db_type;
		ASSERT(bp->b_wptr >= bp->b_rptr);
		count = bp->b_wptr - bp->b_rptr;
		endbp = bp;
		while (bp != mp) {
			for (bp = mp; bp->b_cont != endbp; bp = bp->b_cont)
				;
			if (type != bp->b_datap->db_type)
				break;
			ASSERT(bp->b_wptr >= bp->b_rptr);
			count += bp->b_wptr - bp->b_rptr;
			endbp = bp;
		}
	}
	return count;
}

/*
 * Concatenate and align first len bytes of common message type.
 *
 * Len == -1  means concatenate everything.
 * Returns 1 on success, 0 on failure.
 * After pullup, mp points to the pulled up data.
 */
int
pullupmsg(mblk_t *mp, int len)
{
	struct lf_listvec *lf_listvecp;
	struct msgb *bp, *bcont, *newmp;
	struct datab *datap;
	int index, n, savelen;
#ifdef DEBUG
	int newlen;
#endif /* DEBUG */
	caddr_t newbuf, newbuf_wp;
	cpuid_t cpu_id;

	ASSERT(mp != NULL);
	STR_CALLSTATS(pullupmsg_calls);
	/*
	 * special cases
	 */
	if (len == -1)	{
		if (mp->b_cont == NULL) {
			if (str_aligned(mp->b_rptr))  {
				STR_CALLSTATS(pullupmsg_m1_aligned);
				return 1; /* success */
			}
			len = (int) (mp->b_wptr - mp->b_rptr);
		} else {
			len = xmsgsize(mp, 1);
		}
	} else {
		ASSERT(mp->b_wptr >= mp->b_rptr);

		if (mp->b_wptr - mp->b_rptr >= len) {

			if (str_aligned(mp->b_rptr)) { /* success */
				STR_CALLSTATS(pullupmsg_notm1_aligned);
				return 1;
			}
			/*
			 * Stretch len to include what's in the first
			 * mblock, so we don't worry about residue in
			 * the first block after copying. The idea is
			 * to avoid having to set aside a new mblock for
			 * that residue. Also, this means, if db_ref==1, we
			 * can free the data buffer.
			 */
			len = mp->b_wptr - mp->b_rptr;
		} else {
			if (xmsgsize(mp, 1) < len) { /* failed */
				STR_CALLSTATS(pullupmsg_xmsgsize_fail);
				return 0;
			}
		}
	}
	ASSERT(mp->b_datap->db_ref > 0);
	datap = mp->b_datap;

	if (atomicAddInt(&(datap->db_ref), -1) > 0) {
		/*
		 * Other references in use; so copy
		 * Get a new "data block" if the current one is shared. 
		 * Otherwise, get only a new databuffer of size len.
		 */
		STR_CALLSTATS(pullupmsg_refcnt);
		newmp = allocb(len, BPRI_MED);
		if (newmp == NULL) { /* failed getting msg/dblk/buffer tuple*/

			/* restore ref count on failure */
			atomicAddInt(&(datap->db_ref), 1);
			return 0; /* failed */
		}
		newmp->b_datap->db_type = datap->db_type;
		SAVE_ALLOCB_ADDRESS(newmp, __return_address);

		/*--- copy the data into the new data buffer ---*/
		bp = mp;	
		while (len && bp) {
			ASSERT(bp->b_wptr >= bp->b_rptr);

			n = min((int)(bp->b_wptr - bp->b_rptr), len);
			ASSERT(n <= newmp->b_datap->db_size);
			ASSERT(newmp->b_wptr + n <= newmp->b_datap->db_lim);

			bcopy(bp->b_rptr, newmp->b_wptr, n);
			newmp->b_wptr += n;
			len -= n;
			bp->b_rptr += n;
			if (bp->b_wptr != bp->b_rptr) 
				break;
			bcont = bp->b_cont;
			if (bp != mp) { /* free if not head block */
				freeb(bp);
			}
			bp = bcont;
		} /* while */

		/* update msg block and the (new) data block headers */
		newmp->b_band = mp->b_band; /* we don't care about message */
					    /* header *newmp; but the band */
					    /* and flag fields are in the  */
		newmp->b_flag = mp->b_flag; /* data block in some cases,   */
					    /* so we copy them             */

		if (datap->db_msgaddr == mp) { /* it's ours */
			newmp->b_datap->db_msgaddr = mp;
		}
		mp->b_datap = newmp->b_datap;
		mp->b_rptr = newmp->b_rptr;
		mp->b_wptr = newmp->b_wptr;
		mp->b_cont = bp;

	} else { /* refcnt == 0 => we're only reference (pre-decremented) */

		STR_CALLSTATS(pullupmsg_refcnt_zero);

		/* adjust back refcnt in non-copy case */
		atomicAddInt(&(datap->db_ref), 1);

		index = str_getbuf_index(len);
#ifdef DEBUG
		newlen = str_buffersizes[index];
		ASSERT(newlen >= len);
		ASSERT((newlen >= STR_BUF64_SIZE) && (newlen <= NBPP));
#endif /* DEBUG */
		cpu_id = datap->db_cpuid;
		lf_listvecp = STR_LFVEC(cpu_id);

		newbuf = (caddr_t)lf_get(lf_listvecp, index, cpu_id,
				S_DONTWAIT);

		if (newbuf == NULL) { /* failed to get buffer */
			str_stats_ops(cpu_id, index, STR_BUFFER_FAILOPS, 1);
			return 0;
		}
		str_stats_ops(cpu_id, index, STR_BUFFER_OPS, 1);
		/*
		 * we can't use the db_size field to remember the new
		 * data buffer size. Because we use db_size to detect if
		 * stream does weird things with the buffer that can screw
		 * up zone when we free memory.
		 */
		savelen = len;
		/*
		 * Copy data into the new data buffer.
		 */
		newbuf_wp = newbuf;	
		bp = mp;

		while (len && bp) {
			ASSERT(bp->b_wptr >= bp->b_rptr);

			n = min((int)(bp->b_wptr - bp->b_rptr), len);
#ifdef DEBUG
			ASSERT(n <= newlen);
			ASSERT((newbuf_wp + n) <= (newbuf + newlen));
#endif /* DEBUG */
			bcopy(bp->b_rptr, newbuf_wp, n);
			newbuf_wp += n;
			len -= n;
			bp->b_rptr += n;

			if (bp->b_wptr != bp->b_rptr) {
				break;
			}
			bcont = bp->b_cont;
			if (bp != mp) { /* free if not head block */
				freeb(bp);
			}
			bp = bcont;
		} /* end while */
		mp->b_cont = bp;
		/*
		 * Release previous data buffer;
		 * NOTE: The buffers are now ALWAYS separate and
		 * non-contiguous from msg/dblk headers, unlike in
		 * previous versions!
		 * Also the specific free_func in the dblk MUST be MP-safe!
		 */
		if (datap->db_frtnp == NULL) {
			ASSERT(((char *)datap->db_lim -
				(char *)datap->db_base) == datap->db_size);
			ASSERT(datap->db_index >= STR_BUF64_INDEX);

			lf_free(lf_listvecp, (int)(datap->db_index),
				(struct lf_elt *)(datap->db_base), cpu_id);

			str_stats_ops(cpu_id, (int)(datap->db_index),
				STR_BUFFER_OPS, -1);
		} else {
			if (datap->db_frtnp->free_func) {
				(*datap->db_frtnp->free_func)
					(datap->db_frtnp->free_arg);
			}
		}
		/*
		 * Update header information.
		 */
		mp->b_rptr = (unsigned char *)newbuf;
		mp->b_wptr = (unsigned char *)newbuf_wp;

		datap->db_frtnp = NULL;
		datap->db_base = (unsigned char *)newbuf;
		datap->db_lim = (unsigned char *)newbuf + savelen;
		datap->db_ref = 1;
		datap->db_size = savelen;
		/*
		 * NOTE: The db_type field MUST not be reset since the pullup
		 * works only for messages of the same type.
		 * We must reset buffer cpuid & index since they could
		 * be different.
		 */
		datap->db_buf_cpuid = cpu_id;
		datap->db_index = (unsigned char)(index & 0xff);

		datap->db_frtn.free_func = NULL;
		datap->db_frtn.free_arg = NULL;
	}
	return 1; /* success */
}

/*
 * get number of data bytes in message
 */
int
msgdsize(mblk_t *bp)
{
	int count = 0;

	STR_CALLSTATS(msgdsize_calls);
	for (; bp; bp = bp->b_cont) {
		if (bp->b_datap->db_type == M_DATA) {
			ASSERT(bp->b_wptr >= bp->b_rptr);
			count += bp->b_wptr - bp->b_rptr;
		}
	}
	return count;
}

/*
 * get number of data bytes in message
 */
int
__str_msgsize(mblk_t *bp)
{
	int count = 0;

	STR_CALLSTATS(str_msgsize_calls);
	for (; bp; bp = bp->b_cont) {
		ASSERT(bp->b_wptr >= bp->b_rptr);
		count += bp->b_wptr - bp->b_rptr;
	}
	return count;
}

/*
 * Return number of pages current in use by the Streams sub-system.
 */
__uint32_t
str_getpage_usage(void)
{
	return str_page_cur;
}

/*
 * DDI/DKI replacement for pullupmsg.
 *
 * Concatenate and align first len bytes of common message type.
 * Len == -1  means concatenate everything.
 * 
 * Returns pointer to new pulled-up message, or NULL on failure.
 */
mblk_t *
msgpullup(mblk_t *mp, int len)
{
	register mblk_t *bp, *newmp, *tmp;
	register int n, partial;

	ASSERT(mp != NULL);
	/*
	 * special cases
	 */
	if (len == -1) { /* catenate all so set len to proper size */
		if (mp->b_cont == NULL) {
			if (str_aligned(mp->b_rptr)) {
				return(copyb(mp));
			}
			len = (int)(mp->b_wptr - mp->b_rptr);
		} else {
			len = xmsgsize(mp, 1);
		}
	} else {
		ASSERT(mp->b_wptr >= mp->b_rptr);
		if (mp->b_wptr - mp->b_rptr >= len) {
			if (str_aligned(mp->b_rptr)) {
				return(copymsg(mp));
			}
		} else {
			if (xmsgsize(mp, 1) < len) {
				return NULL;
			}
		}
	}
	/* Get new mblk/dblk/buffer trio */
	if ((newmp = allocb(len, BPRI_MED)) == NULL) { /* failed */
		return NULL;
	}

	/* Copy over important info */
	newmp->b_datap->db_type = mp->b_datap->db_type;
	newmp->b_flag = mp->b_flag;
	newmp->b_band = mp->b_band;

	/* Copy the data into the new message */
	bp = mp;	
	partial = 0;
	while (len) {
		ASSERT(bp);
		ASSERT(bp->b_wptr >= bp->b_rptr);

		n = min((int)(bp->b_wptr - bp->b_rptr), len);
		ASSERT(n <= newmp->b_datap->db_size);
		ASSERT((newmp->b_wptr + n) <=
		       (newmp->b_datap->db_base + newmp->b_datap->db_size));

		bcopy(bp->b_rptr, newmp->b_wptr, n);
		newmp->b_wptr += n;
		len -= n;
		if (!len && (bp->b_rptr + n) != bp->b_wptr) {
			partial++;
			break;
		}
		bp = bp->b_cont;
	}
	/*
	 * Everything the caller wanted "pulled-up" has been "pulled-up"
	 * into the new message block.  If there is any residual data left
	 * over, we need to make a copy of it and attach it to the new
	 * message block as a continuation.
	 *
	 * If bp != NULL, it points to a mblk that was not or was only
	 * partially "pulled-up".  In partial case, "partial" will be set
	 * non-zero, and n is the number of bytes already "pulled-up".
	 */
	if (bp != NULL) {
		if ((tmp = copymsg(bp)) == NULL) {
			freeb(newmp);
			return NULL;
		}
		if (partial) {
			tmp->b_rptr += n;
		}
		newmp->b_cont = tmp;
	}
	return newmp;
}

/*
 * Get a message off head of queue
 *
 * If queue has no buffers then mark queue
 * with QWANTR. (queue wants to be read by
 * someone when data becomes available)
 *
 * If there is something to take off then do so.
 * If queue falls below hi water mark turn off QFULL
 * flag.  Decrement weighted count of queue.
 * Also turn off QWANTR because queue is being read.
 *
 * The queue count is maintained on a per-band basis.
 * Priority band 0 (normal messages) uses q_count,
 * q_lowat, etc.  Non-zero priority bands use the
 * fields in their respective qband structures
 * (qb_count, qb_lowat, etc.)  All messages appear
 * on the same list, linked via their b_next pointers.
 * q_first is the head of the list.  q_count does
 * not reflect the size of all the messages on the
 * queue.  It only reflects those messages in the
 * normal band of flow.  The one exception to this
 * deals with high priority messages.  They are in
 * their own conceptual "band", but are accounted
 * against q_count.
 *
 * If queue count is below the lo water mark and QWANTW
 * is set, enable the closest backq which has a service 
 * procedure and turn off the QWANTW flag.
 *
 * getq could be built on top of rmvq, but isn't because
 * of performance considerations.
 */
mblk_t *
getq(register queue_t *q)
{
	register mblk_t *bp, *tmp;
	register qband_t *qbp = (qband_t *) NULL;
	int backenable = 0;
	int wantw = 0;
	queue_t *nq;

	ASSERT(q);
	ASSERT(STREAM_LOCKED(q));

	if (!(bp = q->q_first)) {
		q->q_flag |= QWANTR;
		backenable = 1;		/* we might backenable */
		wantw = q->q_flag & QWANTW;
	} else {
		if (bp->b_flag & MSGNOGET) {	/* hack hack hack */
			while (bp && (bp->b_flag & MSGNOGET))
				bp = bp->b_next;
			if (bp)
				rmvq(q, bp);
			return (bp);
		}
		if (!(q->q_first = bp->b_next))
			q->q_last = NULL;
		else
			q->q_first->b_prev = NULL;

		if (bp->b_band == 0) {
#ifdef _STRQ_TRACING
			if (q->q_flag & QTRC)
			  q_trace_getq(q, bp, (inst_t *)__return_address);
#endif
			wantw = q->q_flag & QWANTW;
#ifdef _STRQ_TRACING
			if (q->q_flag & QTRC)
				q_trace_sub_1(q, (inst_t *)__return_address);
#endif
			for (tmp = bp; tmp; tmp = tmp->b_cont)
				q->q_count -= (tmp->b_wptr - tmp->b_rptr);
#ifdef _STRQ_TRACING
			if (q->q_flag & QTRC)
				q_trace_sub_2(q, (inst_t *)__return_address);
#endif
			if (q->q_count < q->q_hiwat)
				q->q_flag &= ~QFULL;
			if (q->q_count <= q->q_lowat)
				backenable = 1;
		} else {
			unsigned char i;

			ASSERT(bp->b_band <= q->q_nband);
			ASSERT(q->q_bandp != NULL);
			qbp = q->q_bandp;
			i = bp->b_band;
			while (--i > 0)
				qbp = qbp->qb_next;
			if (qbp->qb_first == qbp->qb_last) {
				qbp->qb_first = NULL;
				qbp->qb_last = NULL;
			} else {
				qbp->qb_first = bp->b_next;
			}
			wantw = qbp->qb_flag & QB_WANTW;
			for (tmp = bp; tmp; tmp = tmp->b_cont)
				qbp->qb_count -= (tmp->b_wptr - tmp->b_rptr);
			if ((qbp->qb_count < qbp->qb_hiwat) &&
			    (qbp->qb_flag & QB_FULL)) {
				qbp->qb_flag &= ~QB_FULL;
				q->q_blocked--;
			}
			if (qbp->qb_count <= qbp->qb_lowat)
				backenable = 1;
		}
		q->q_flag &= ~QWANTR;
		bp->b_next = NULL;
		bp->b_prev = NULL;
	}
	if (backenable) {
		/* find nearest back queue with service proc */
		for (nq = backq(q); nq && !nq->q_qinfo->qi_srvp;
		     nq = backq(nq))
			;
		if (wantw) {
			if (bp && bp->b_band != 0)
				qbp->qb_flag &= ~QB_WANTW;
			else
				q->q_flag &= ~QWANTW;
			if (nq) {
				qenable(nq);
				setqback(nq, bp ? bp->b_band : 0);
			}
		}
		if ((q->q_flag & QWANTW) && (q->q_blocked == 0) &&
		    !(q->q_flag & QFULL) && nq) {
			qenable(nq);
			setqback(nq, 0);
		}
	}
	return(bp);
}

/*
 * Remove a message from a queue.  The queue count and other flow control
 * parameters are adjusted and the back queue enabled if necessary.
 */
void
rmvq(register queue_t *q, register mblk_t *mp)
{
	register mblk_t *tmp;
	register int i;
	register qband_t *qbp = NULL;
	int backenable = 0;
	int wantw = 0;
	queue_t *nq;

	ASSERT(q);
	ASSERT(mp);
	ASSERT(STREAM_LOCKED(q));

#ifdef _STRQ_TRACING
	if (q->q_flag & QTRC)
		q_trace_rmvq(q, mp, (inst_t *)__return_address);
#endif
	ASSERT(mp->b_band <= q->q_nband);
	if (mp->b_band != 0) {		/* Adjust band pointers */
		ASSERT(q->q_bandp != NULL);
		qbp = q->q_bandp;
		i = mp->b_band;
		while (--i > 0)
			qbp = qbp->qb_next;
		if (mp == qbp->qb_first) {
			if (mp->b_next && mp->b_band == mp->b_next->b_band)
				qbp->qb_first = mp->b_next;
			else
				qbp->qb_first = NULL;
		}
		if (mp == qbp->qb_last) {
			if (mp->b_prev && mp->b_band == mp->b_prev->b_band)
				qbp->qb_last = mp->b_prev;
			else
				qbp->qb_last = NULL;
		}
	}

	/*
	 * Remove the message from the list.
	 */
	if (mp->b_prev)
		mp->b_prev->b_next = mp->b_next;
	else
		q->q_first = mp->b_next;
	if (mp->b_next)
		mp->b_next->b_prev = mp->b_prev;
	else
		q->q_last = mp->b_prev;
	mp->b_next = NULL;
	mp->b_prev = NULL;

	if (mp->b_band == 0) {		/* Perform q_count accounting */
		wantw = q->q_flag & QWANTW;
#ifdef _STRQ_TRACING
		if (q->q_flag & QTRC)
			q_trace_sub_1(q, (inst_t *)__return_address);
#endif
		for (tmp = mp; tmp; tmp = tmp->b_cont)
			q->q_count -= (tmp->b_wptr - tmp->b_rptr);
#ifdef _STRQ_TRACING
		if (q->q_flag & QTRC)
			q_trace_sub_2(q, (inst_t *)__return_address);
#endif
		if (q->q_count < q->q_hiwat)
			q->q_flag &= ~QFULL;
		if (q->q_count <= q->q_lowat)
			backenable = 1;
	} else {			/* Perform qb_count accounting */
		wantw = qbp->qb_flag & QB_WANTW;
		for (tmp = mp; tmp; tmp = tmp->b_cont)
			qbp->qb_count -= (tmp->b_wptr - tmp->b_rptr);
		if ((qbp->qb_count < qbp->qb_hiwat) &&
		    (qbp->qb_flag & QB_FULL)) {
			qbp->qb_flag &= ~QB_FULL;
			q->q_blocked--;
		}
		if (qbp->qb_count <= qbp->qb_lowat)
			backenable = 1;
	}

	if (backenable) {
		/* find nearest back queue with service proc */
		for(nq = backq(q); nq && !nq->q_qinfo->qi_srvp; nq = backq(nq))
			;
		if (wantw) {
			if (mp->b_band != 0)
				qbp->qb_flag &= ~QB_WANTW;
			else
				q->q_flag &= ~QWANTW;
			if (nq) {
				qenable(nq);
				setqback(nq, mp->b_band);
			}
		}
		if ((q->q_flag & QWANTW) && (q->q_blocked == 0) &&
		    !(q->q_flag & QFULL) && nq) {
			qenable(nq);
			setqback(nq, 0);
		}
	}
	return;
}

/*
 * Empty a queue.  
 * If flag is set, remove all messages.  Otherwise, remove
 * only non-control messages.  If queue falls below its low
 * water mark, and QWANTW is set , enable the nearest upstream
 * service procedure.
 */
void
flushq(register queue_t *q, int flag)
{
	register mblk_t *mp, *nmp;
	register qband_t *qbp;
	int backenable = 0;
	unsigned char bpri, i;
	unsigned char qbf[NBAND];


	ASSERT(q);
	ASSERT(STREAM_LOCKED(q));

	mp = q->q_first;
	q->q_first = NULL;
	q->q_last = NULL;
	q->q_count = 0;
#ifdef _STRQ_TRACING
	if (q->q_flag & QTRC)
		q_trace_zero(q, (inst_t *)__return_address);
#endif
	for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next) {
		qbp->qb_first = NULL;
		qbp->qb_last = NULL;
		qbp->qb_count = 0;
		qbp->qb_flag &= ~QB_FULL;
	}
	q->q_blocked = 0;
	q->q_flag &= ~QFULL;
	while (mp) {
		nmp = mp->b_next;
		if (flag || datamsg(mp->b_datap->db_type))
			freemsg(mp);
		else
			putq(q, mp);
		mp = nmp;
	}
	bzero((caddr_t)qbf, NBAND);
	bpri = 1;
	for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next) {
		if ((qbp->qb_flag & QB_WANTW) && (qbp->qb_count <= qbp->qb_lowat)) {
			qbp->qb_flag &= ~QB_WANTW;
			backenable = 1;
			qbf[bpri] = 1;
		}
		bpri++;
	}
	if ((q->q_flag & QWANTW) && (q->q_count <= q->q_lowat)) {
		q->q_flag &= ~QWANTW;
		backenable = 1;
		qbf[0] = 1;
	}

	/*
	 * If any band can now be written to, and there is a writer
	 * for that band, then backenable the closest service procedure.
	 */
	if (backenable) {
		/* find nearest back queue with service proc */
		for (q = backq(q); q && !q->q_qinfo->qi_srvp; q = backq(q))
			;
		if (q) {
			qenable(q);
			for (i = 0; i < bpri; i++)
				if (qbf[i])
					setqback(q, i);
		}
	}
	return;
}

/*
 * Flush the queue of messages of the given priority band.
 * There is some duplication of code between flushq and flushband.
 * This is because we want to optimize the code as much as possible.
 * The assumption is that there will be more messages in the normal
 * (priority 0) band than in any other.
 */
void
flushband(register queue_t *q, unsigned char pri, int flag)
{
	register mblk_t *mp, *nmp, *last;
	register qband_t *qbp;
	int band;

	ASSERT(q);
	ASSERT(STREAM_LOCKED(q));

	if (pri > q->q_nband) {
		return;
	}
	if (pri == 0) {
		mp = q->q_first;
		q->q_first = NULL;
		q->q_last = NULL;
		q->q_count = 0;
#ifdef _STRQ_TRACING
		if (q->q_flag & QTRC)
			q_trace_zero(q, (inst_t *)__return_address);
#endif
		for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next) {
			qbp->qb_first = NULL;
			qbp->qb_last = NULL;
			qbp->qb_count = 0;
			qbp->qb_flag &= ~QB_FULL;
		}
		q->q_blocked = 0;
		q->q_flag &= ~QFULL;
		while (mp) {
			nmp = mp->b_next;
			if ((mp->b_band == 0) &&
			    (mp->b_datap->db_type < QPCTL) &&
			    (flag || datamsg(mp->b_datap->db_type)))
				freemsg(mp);
			else
				putq(q, mp);
			mp = nmp;
		}
		if ((q->q_flag & QWANTW) && (q->q_count <= q->q_lowat)) {
			/* find nearest back queue with service proc */
			q->q_flag &= ~QWANTW;
			for (q = backq(q); q && !q->q_qinfo->qi_srvp; q = backq(q))
				;
			if (q) {
				qenable(q);
				setqback(q, pri);
			}
		}
	} else {	/* pri != 0 */
		ASSERT(q->q_bandp != NULL);
		band = pri;
		qbp = q->q_bandp;
		while (--band > 0)
			qbp = qbp->qb_next;
		mp = qbp->qb_first;
		if (mp == NULL) {
			return;
		}
		last = qbp->qb_last;
		if (mp == last)		/* only message in band */
			last = mp->b_next;
		while (mp != last) {
			nmp = mp->b_next;
			if (mp->b_band == pri) {
				if (flag || datamsg(mp->b_datap->db_type)) {
					rmvq(q, mp);
					freemsg(mp);
				}
			}
			mp = nmp;
		}
		if (mp && mp->b_band == pri) {
			if (flag || datamsg(mp->b_datap->db_type)) {
				rmvq(q, mp);
				freemsg(mp);
			}
		}
	}
	return;
}

/*
 * Return 1 if the queue is not full.  If the queue is full, return
 * 0 (may not put message) and set QWANTW flag (caller wants to write
 * to the queue).
 */
int
canput(register queue_t *q)
{
	if (!q)
		return(0);

	ASSERT(STREAM_LOCKED(q));

	while (q->q_next && !q->q_qinfo->qi_srvp)
		q = q->q_next;
	if ((q->q_flag & QFULL) || (q->q_blocked != 0)) {
		q->q_flag |= QWANTW;
		return(0);
	}
	return(1);
}

/*
 * This is the new canput for use with priority bands.  Return 1 if the
 * band is not full.  If the band is full, return 0 (may not put message)
 * and set QWANTW(QB_WANTW) flag for zero(non-zero) band (caller wants to
 * write to the queue).
 */
int
bcanput(register queue_t *q, unsigned char pri)
{
	register qband_t *qbp;

	if (!q)
		return(0);

	ASSERT(STREAM_LOCKED(q));

	while (q->q_next && !q->q_qinfo->qi_srvp)
		q = q->q_next;
	if (pri == 0) {
		if (q->q_flag & QFULL) {
			q->q_flag |= QWANTW;
			return(0);
		}
	} else {	/* pri != 0 */
		if (pri > q->q_nband) {
			/*
			 * No band exists yet, so return success.
			 */
			return(1);
		}
		qbp = q->q_bandp;
		while (--pri)
			qbp = qbp->qb_next;
		if (qbp->qb_flag & QB_FULL) {
			qbp->qb_flag |= QB_WANTW;
			return(0);
		}
	}
	return(1);
}

/*
 * Put a message on a queue.  
 *
 * Messages are enqueued on a priority basis.  The priority classes
 * are HIGH PRIORITY (type >= QPCTL), PRIORITY (type < QPCTL && band > 0),
 * and B_NORMAL (type < QPCTL && band == 0). 
 *
 * Add appropriate weighted data block sizes to queue count.
 * If queue hits high water mark then set QFULL flag.
 *
 * If QNOENAB is not set (putq is allowed to enable the queue),
 * enable the queue only if the message is PRIORITY,
 * or the QWANTR flag is set (indicating that the service procedure 
 * is ready to read the queue.  This implies that a service 
 * procedure must NEVER put a priority message back on its own
 * queue, as this would result in an infinite loop (!).
 */
int
putq(register queue_t *q, register mblk_t *bp)
{
	register mblk_t *tmp;
	register qband_t *qbp = NULL;
	int mcls = (int)queclass(bp);

	ASSERT(q && bp);
	ASSERT(STREAM_LOCKED(q));
	SAVE_PUTQ_QADDRESS(bp, q);

#ifdef _STRQ_TRACING
	if (q->q_flag & QTRC)
		q_trace_putq(q, bp, (inst_t *)__return_address);
#endif
	/*
	 * Make sanity checks and if qband structure is not yet
	 * allocated, do so.
	 */
	if (mcls == QPCTL) {
		if (bp->b_band != 0)
			bp->b_band = 0;		/* force to be correct */
	} else if (bp->b_band != 0) {
		register int i;
		qband_t **qbpp;

		if (bp->b_band > q->q_nband) {
			/*
			 * The qband structure for this priority band is
			 * not on the queue yet, so we have to allocate
			 * one on the fly.  It would be wasteful to
			 * associate the qband structures with every
			 * queue when the queues are allocated.  This is
			 * because most queues will only need the normal
			 * band of flow which can be described entirely
			 * by the queue itself.
			 */
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (bp->b_band > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					return(0);
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = bp->b_band;
		while (--i)
			qbp = qbp->qb_next;
	}

	/*
	 * If queue is empty, add the message and initialize the pointers.
	 * Otherwise, adjust message pointers and queue pointers based on
	 * the type of the message and where it belongs on the queue.  Some
	 * code is duplicated to minimize the number of conditionals and
	 * hopefully minimize the amount of time this routine takes.
	 */
	if (!q->q_first) {
		bp->b_next = NULL;
		bp->b_prev = NULL;
		q->q_first = bp;
		q->q_last = bp;
		if (qbp) {
			qbp->qb_first = bp;
			qbp->qb_last = bp;
		}
	} else if (!qbp) {	/* bp->b_band == 0 */

		/* 
		 * If queue class of message is less than or equal to
		 * that of the last one on the queue, tack on to the end.
		 */
		tmp = q->q_last;
		if (mcls <= (int)queclass(tmp)) {
			bp->b_next = NULL;
			bp->b_prev = tmp;
			tmp->b_next = bp;
			q->q_last = bp;
		} else {
			tmp = q->q_first;
			while ((int)queclass(tmp) >= mcls)
				tmp = tmp->b_next;
			ASSERT(tmp != NULL);

			/*
			 * Insert bp before tmp.
			 */
			bp->b_next = tmp;
			bp->b_prev = tmp->b_prev;
			if (tmp->b_prev)
				tmp->b_prev->b_next = bp;
			else
				q->q_first = bp;
			tmp->b_prev = bp;
		}
	} else {		/* bp->b_band != 0 */
		if (qbp->qb_first) {
			ASSERT(qbp->qb_last != NULL);
			tmp = qbp->qb_last;

			/*
			 * Insert bp after the last message in this band.
			 */
			bp->b_next = tmp->b_next;
			if (tmp->b_next)
				tmp->b_next->b_prev = bp;
			else
				q->q_last = bp;
			bp->b_prev = tmp;
			tmp->b_next = bp;
		} else {
			tmp = q->q_last;
			if ((mcls < (int)queclass(tmp)) ||
			    (bp->b_band <= tmp->b_band)) {

				/*
				 * Tack bp on end of queue.
				 */
				bp->b_next = NULL;
				bp->b_prev = tmp;
				tmp->b_next = bp;
				q->q_last = bp;
			} else {
				tmp = q->q_first;
				while (tmp->b_datap->db_type >= QPCTL)
					tmp = tmp->b_next;
				while (tmp->b_band >= bp->b_band)
					tmp = tmp->b_next;
				ASSERT(tmp != NULL);

				/*
				 * Insert bp before tmp.
				 */
				bp->b_next = tmp;
				bp->b_prev = tmp->b_prev;
				if (tmp->b_prev)
					tmp->b_prev->b_next = bp;
				else
					q->q_first = bp;
				tmp->b_prev = bp;
			}
			qbp->qb_first = bp;
		}
		qbp->qb_last = bp;
	}

	if (qbp) {
		for (tmp = bp; tmp; tmp = tmp->b_cont)
			qbp->qb_count += (tmp->b_wptr - tmp->b_rptr);
		if ((qbp->qb_count >= qbp->qb_hiwat) &&
		    !(qbp->qb_flag & QB_FULL)) {
			qbp->qb_flag |= QB_FULL;
			q->q_blocked++;
		}
	} else {
#ifdef _STRQ_TRACING
		if (q->q_flag & QTRC)
			q_trace_add_1(q, (inst_t *)__return_address);
#endif
		for (tmp = bp; tmp; tmp = tmp->b_cont)
			q->q_count += (tmp->b_wptr - tmp->b_rptr);
#ifdef _STRQ_TRACING
		if (q->q_flag & QTRC)
			q_trace_add_2(q, (inst_t *)__return_address);
#endif
		if (q->q_count >= q->q_hiwat)
			q->q_flag |= QFULL;
	}
	if ((mcls > QNORM) || (canenable(q) && (q->q_flag & QWANTR)))
		qenable(q);

	return(1);
}

/*
 * Put stuff back at beginning of Q according to priority order.
 * See comment on putq above for details.
 */
int
putbq(register queue_t *q, register mblk_t *bp)
{
	register mblk_t *tmp;
	register qband_t *qbp = NULL;
	int mcls = (int)queclass(bp);

	ASSERT(q && bp);
	ASSERT(bp->b_next == NULL);
	ASSERT(STREAM_LOCKED(q));

#ifdef _STRQ_TRACING
	if (q->q_flag & QTRC)
		q_trace_putbq(q, bp, (inst_t *)__return_address);
#endif
	/*
	 * Make sanity checks and if qband structure is not yet
	 * allocated, do so.
	 */
	if (mcls == QPCTL) {
		if (bp->b_band != 0)
			bp->b_band = 0;		/* force to be correct */
	} else if (bp->b_band != 0) {
		register int i;
		qband_t **qbpp;

		if (bp->b_band > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (bp->b_band > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					return(0);
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = bp->b_band;
		while (--i)
			qbp = qbp->qb_next;
	}

	/* 
	 * If queue is empty or if message is high priority,
	 * place on the front of the queue.
	 */
	tmp = q->q_first;
	if ((!tmp) || (mcls == QPCTL)) {
		bp->b_next = tmp;
		if (tmp)
			tmp->b_prev = bp;
		else
			q->q_last = bp;
		q->q_first = bp;
		bp->b_prev = NULL;
		if (qbp) {
			qbp->qb_first = bp;
			qbp->qb_last = bp;
		}
	} else if (qbp) {	/* bp->b_band != 0 */
		tmp = qbp->qb_first;
		if (tmp) {
			/*
			 * Insert bp before the first message in this band.
			 */
			bp->b_next = tmp;
			bp->b_prev = tmp->b_prev;
			if (tmp->b_prev)
				tmp->b_prev->b_next = bp;
			else
				q->q_first = bp;
			tmp->b_prev = bp;
		} else {
			tmp = q->q_last;
			if ((mcls < (int)queclass(tmp)) ||
			    (bp->b_band < tmp->b_band)) {

				/*
				 * Tack bp on end of queue.
				 */
				bp->b_next = NULL;
				bp->b_prev = tmp;
				tmp->b_next = bp;
				q->q_last = bp;
			} else {
				tmp = q->q_first;
				while (tmp->b_datap->db_type >= QPCTL)
					tmp = tmp->b_next;
				while (tmp->b_band > bp->b_band)
					tmp = tmp->b_next;
				ASSERT(tmp != NULL);

				/*
				 * Insert bp before tmp.
				 */
				bp->b_next = tmp;
				bp->b_prev = tmp->b_prev;
				if (tmp->b_prev)
					tmp->b_prev->b_next = bp;
				else
					q->q_first = bp;
				tmp->b_prev = bp;
			}
			qbp->qb_last = bp;
		}
		qbp->qb_first = bp;
	} else {		/* bp->b_band == 0 && !QPCTL */

		/*
		 * If the queue class or band is less than that of the last
		 * message on the queue, tack bp on the end of the queue.
		 */
		tmp = q->q_last;
		if ((mcls < (int)queclass(tmp)) || (bp->b_band < tmp->b_band)){
			bp->b_next = NULL;
			bp->b_prev = tmp;
			tmp->b_next = bp;
			q->q_last = bp;
		} else {
			tmp = q->q_first;
			while (tmp->b_datap->db_type >= QPCTL)
				tmp = tmp->b_next;
			while (tmp->b_band > bp->b_band)
				tmp = tmp->b_next;
			ASSERT(tmp != NULL);

			/*
			 * Insert bp before tmp.
			 */
			bp->b_next = tmp;
			bp->b_prev = tmp->b_prev;
			if (tmp->b_prev)
				tmp->b_prev->b_next = bp;
			else
				q->q_first = bp;
			tmp->b_prev = bp;
		}
	}

	if (qbp) {
		for (tmp = bp; tmp; tmp = tmp->b_cont)
			qbp->qb_count += (tmp->b_wptr - tmp->b_rptr);
		if ((qbp->qb_count >= qbp->qb_hiwat) &&
		    !(qbp->qb_flag & QB_FULL)) {
			qbp->qb_flag |= QB_FULL;
			q->q_blocked++;
		}
	} else {
#ifdef _STRQ_TRACING
		if (q->q_flag & QTRC)
			q_trace_add_1(q, (inst_t *)__return_address);
#endif
		for (tmp = bp; tmp; tmp = tmp->b_cont)
			q->q_count += (tmp->b_wptr - tmp->b_rptr);
#ifdef _STRQ_TRACING
		if (q->q_flag & QTRC)
			q_trace_add_2(q, (inst_t *)__return_address);
#endif
		if (q->q_count >= q->q_hiwat)
			q->q_flag |= QFULL;
	}
	if ((mcls > QNORM) || (canenable(q) && (q->q_flag & QWANTR)))
		qenable(q);

	return(1);
}

/*
 * Insert a message before an existing message on the queue.  If the
 * existing message is NULL, the new messages is placed on the end of
 * the queue.  The queue class of the new message is ignored.  However,
 * the priority band of the new message must adhere to the following
 * ordering:
 *
 *	emp->b_prev->b_band >= mp->b_band >= emp->b_band.
 *
 * All flow control parameters are updated.
 */
int
insq(register queue_t *q, register mblk_t *emp, register mblk_t *mp)
{
	register mblk_t *tmp;
	register qband_t *qbp = NULL;
	int mcls = (int)queclass(mp);

	ASSERT(STREAM_LOCKED(q));

#ifdef _STRQ_TRACING
	if (q->q_flag & QTRC)
		q_trace_insq(q, mp, emp, (inst_t *)__return_address);
#endif
	if (mcls == QPCTL) {
		if (mp->b_band != 0)
			mp->b_band = 0;		/* force to be correct */
		if (emp && emp->b_prev &&
		    (emp->b_prev->b_datap->db_type < QPCTL))
			goto badord;
	}
	if (emp) {
		if (((mcls == QNORM) && (mp->b_band < emp->b_band)) ||
		    (emp->b_prev && (emp->b_prev->b_datap->db_type < QPCTL) &&
		    (emp->b_prev->b_band < mp->b_band))) {
			goto badord;
		}
	} else {
		tmp = q->q_last;
		if (tmp && (mcls == QNORM) && (mp->b_band > tmp->b_band)) {
badord:
			cmn_err_tag(165,CE_WARN,
		"insq: attempt to insert message out of order on q %x\n", q);
			return(0);
		}
	}

	if (mp->b_band != 0) {
		register int i;
		qband_t **qbpp;

		if (mp->b_band > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (mp->b_band > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					return(0);
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = mp->b_band;
		while (--i)
			qbp = qbp->qb_next;
	}

	if (mp->b_next = emp) {
		if (mp->b_prev = emp->b_prev) 
			emp->b_prev->b_next = mp;
		else
			q->q_first = mp;
		emp->b_prev = mp;
	} else {
		if (mp->b_prev = q->q_last) 
			q->q_last->b_next = mp;
		else 
			q->q_first = mp;
		q->q_last = mp;
	}

	if (qbp) { /* adjust qband pointers and count */
		if (!qbp->qb_first) {
			qbp->qb_first = mp;
			qbp->qb_last = mp;
		} else {
			if (qbp->qb_first == emp)
				qbp->qb_first = mp;
			else if (mp->b_next && (mp->b_next->b_band !=
			    mp->b_band))
				qbp->qb_last = mp;
		}
		for (tmp = mp; tmp; tmp = tmp->b_cont)
			qbp->qb_count += (tmp->b_wptr - tmp->b_rptr);
		if ((qbp->qb_count >= qbp->qb_hiwat) &&
		    !(qbp->qb_flag & QB_FULL)) {
			qbp->qb_flag |= QB_FULL;
			q->q_blocked++;
		}
	} else {
#ifdef _STRQ_TRACING
		if (q->q_flag & QTRC)
			q_trace_add_1(q, (inst_t *)__return_address);
#endif
		for (tmp = mp; tmp; tmp = tmp->b_cont)
			q->q_count += (tmp->b_wptr - tmp->b_rptr);
#ifdef _STRQ_TRACING
		if (q->q_flag & QTRC)
			q_trace_add_2(q, (inst_t *)__return_address);
#endif
		if (q->q_count >= q->q_hiwat)
			q->q_flag |= QFULL;
	}
	if (canenable(q) && (q->q_flag & QWANTR))
		qenable(q);

	return(1);
}

/*
 * Create and put a control message on queue.
 */
int
putctl(queue_t *q, int type)
{
	register mblk_t *bp;

	ASSERT(STREAM_LOCKED(q));
	if ((datamsg(type) && (type != M_DELAY)) || !(bp = allocb(0, BPRI_HI)))
		return(0);
	bp->b_datap->db_type = type;
	(*q->q_qinfo->qi_putp)(q, bp);
	return(1);
}

/*
 * Control message with a single-byte parameter
 */
int
putctl1(queue_t *q, int type, int param)
{
	register mblk_t *bp;

	ASSERT(STREAM_LOCKED(q));
	if ((datamsg(type) && (type != M_DELAY)) || !(bp = allocb(1, BPRI_HI)))
		return(0);
	bp->b_datap->db_type = type;
	*bp->b_wptr++ = param;
	(*q->q_qinfo->qi_putp)(q, bp);
	return(1);
}

/*
 * Return the queue upstream from this one
 */
queue_t *
backq(register queue_t *q)
{
	ASSERT(q);

	q = OTHERQ(q);
	if (q->q_next) {
		q = q->q_next;
		return(OTHERQ(q));
	}
	return(NULL);
}

/*
 * Send a block back up the queue in reverse from this
 * one (e.g. to respond to ioctls)
 */
void
qreply(register queue_t *q, mblk_t *bp)
{
	ASSERT(q && bp);

	q = OTHERQ(q);
	ASSERT(q->q_next);
	(*q->q_next->q_qinfo->qi_putp)(q->q_next, bp);
	return;
}

/*
 * Streams Queue Scheduling
 * 
 * Queues are enabled through qenable() when they have messages to 
 * process.  They are serviced by queuerun(), which runs each enabled
 * queue's service procedure.  The call to queuerun() is processor
 * dependent - the general principle is that it be run whenever a queue
 * is enabled but before returning to user level.  For system calls,
 * the function runqueues() is called if their action causes a queue
 * to be enabled.  For device interrupts, queuerun() should be
 * called before returning from the last level of interrupt.  Beyond
 * this, no timing assumptions should be made about queue scheduling.
 */
extern int privenable;
/*
 * Enable a queue: put it on list of those whose service procedures are
 * ready to run and set up the scheduling mechanism.
 */
void
qenable(register queue_t *q)
{
	register int s2;

	ASSERT(q);
	/*
	 * The following ASSERT is a feeble attempt to check the validity
	 * of the queue.  If the condition is false, it proves that the
	 * queue is bad (already freed, garbage queue pointer, etc.), but
	 * if the condition is true, it is no real guarantee that the queue
	 * is good.  It's better than nothing though.
	 */
	ASSERT(q->q_flag & QUSE);
	ASSERT(STREAM_LOCKED(q));

	if (!q->q_qinfo->qi_srvp)
		return;
	/*
	 * Only enable once.
	 */
	LOCK_STR_RESOURCE(s2);
	if (q->q_pflag & QENAB) {
		UNLOCK_STR_RESOURCE(s2);
		return;
	}
	/*
	 * Mark queue enabled and put on run list.
	 */
	q->q_pflag |= QENAB;
	UNLOCK_STR_RESOURCE(s2);

        s2 = STREAMS_ENAB_INTERRUPT(q->q_monpp, q->q_qinfo->qi_srvp, q, 0, 0);
        ASSERT(s2 == 0);
	return;
}

/* 
 * Return number of messages on queue
 */
int
qsize(register queue_t *qp)
{
	register mblk_t *mp;
	register int count = 0;

	ASSERT(qp);

	for (mp = qp->q_first; mp; mp = mp->b_next)
		count++;
	return count;
}

/*
 * noenable - set queue so that putq() will not enable it.
 * enableok - set queue so that putq() can enable it.
 */
void
noenable(queue_t *q)
{ 
	q->q_flag |= QNOENB;
	return;
}

void
enableok(queue_t *q)
{
	q->q_flag &= ~QNOENB;
	return;
}

/*
 * Set queue fields.
 */
int
strqset(queue_t *q, qfields_t what, unsigned char pri, long val)
{
	register qband_t *qbp = NULL;
	int error = 0;

	if (what >= QBAD)
		return (EINVAL);

	if (pri != 0) {
		register int i;
		qband_t **qbpp;

		if (pri > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (pri > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					return (EAGAIN);
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = pri;
		while (--i)
			qbp = qbp->qb_next;
	}
	switch (what) {
	case QHIWAT:
		if (qbp)
			qbp->qb_hiwat = (ulong)val;
		else
			q->q_hiwat = (ulong)val;
		break;

	case QLOWAT:
		if (qbp)
			qbp->qb_lowat = (ulong)val;
		else
			q->q_lowat = (ulong)val;
		break;

	case QMAXPSZ:
		if (qbp)
			error = EINVAL;
		else
			q->q_maxpsz = val;
		break;

	case QMINPSZ:
		if (qbp)
			error = EINVAL;
		else
			q->q_minpsz = val;
		break;

	case QCOUNT:
	case QFIRST:
	case QLAST:
	case QFLAG:
		error = EPERM;
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

/*
 * Get queue fields.
 */
int
strqget(queue_t *q, qfields_t what, unsigned char pri, long *valp)
{
	register qband_t *qbp = NULL;
	int error = 0;

	if (what >= QBAD)
		return (EINVAL);
	if (pri != 0) {
		register int i;
		qband_t **qbpp;

		if (pri > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (pri > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					return (EAGAIN);
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = pri;
		while (--i)
			qbp = qbp->qb_next;
	}

	switch (what) {
	case QHIWAT:
		if (qbp)
			*(ulong *)valp = qbp->qb_hiwat;
		else
			*(ulong *)valp = q->q_hiwat;
		break;

	case QLOWAT:
		if (qbp)
			*(ulong *)valp = qbp->qb_lowat;
		else
			*(ulong *)valp = q->q_lowat;
		break;

	case QMAXPSZ:
		if (qbp)
			error = EINVAL;
		else
			*(long *)valp = q->q_maxpsz;
		break;

	case QMINPSZ:
		if (qbp)
			error = EINVAL;
		else
			*(long *)valp = q->q_minpsz;
		break;

	case QCOUNT:
		if (qbp)
			*(ulong *)valp = qbp->qb_count;
		else
			*(ulong *)valp = q->q_count;
		break;

	case QFIRST:
		if (qbp)
			*(mblk_t **)valp = qbp->qb_first;
		else
			*(mblk_t **)valp = q->q_first;
		break;

	case QLAST:
		if (qbp)
			*(mblk_t **)valp = qbp->qb_last;
		else
			*(mblk_t **)valp = q->q_last;
		break;

	case QFLAG:
		if (qbp)
			*(ulong *)valp = qbp->qb_flag;
		else
			*(ulong *)valp = q->q_flag;
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

/* 
 * Call the put routine of the given queue.
 */
void
put(register queue_t *q, register mblk_t *mp)
{
	(*q->q_qinfo->qi_putp)(q, mp);
	return;
}

/*
 * Test if message is a "priority" type.
 */
int
pcmsg(unsigned char db_type)
{
	return(db_type >= QPCTL);
}

/*
 * DDI/DKI "MP safe" version of bcanput(q->q_next).
 * Since IRIX streams use the streams monitor, this is trivial.
 */
int
bcanputnext(register queue_t *q, unsigned char pri)
{
	return(bcanput(q->q_next, pri));
}

/*
 * DDI/DKI "MP safe" version of canput(q->q_next).
 * Since IRIX streams use the streams monitor, this is trivial.
 */
int
canputnext(register queue_t *q)
{
	return(canput(q->q_next));
}

/*
 * DDI/DKI "MP safe" version of putctl(q->q_next, type).
 * Since IRIX streams use the streams monitor, this is trivial.
 */
int
putnextctl(register queue_t *q, int type)
{
	return(putctl(q->q_next, type));
}

/*
 * DDI/DKI "MP safe" version of putctl1(q->q_next, type, param).
 * Since IRIX streams use the streams monitor, this is trivial.
 */
int
putnextctl1(register queue_t *q, int type, int param)
{
	return(putctl1(q->q_next, type, param));
}

/*
 * DDI/DKI function to enable put/service procedures.  Allows open routine
 * to set up resources in MP environment without having to worry about an
 * "early" call to any put/service procedure.  This is unnecessary for IRIX
 * because of the streams monitor.
 *
 * XXXrs Will any brain-dead test actually try to call the put/service
 *	 procedure before/after calling qprocson?
 */
/* ARGSUSED */
void
qprocson(register queue_t *rq)
{
	return;
}

/*
 * DDI/DKI function to disable put/service procedures.  Allows close routine
 * to tear down resources in MP environment without having to worry about a
 * "late" call to any put/service procedure.  This is unnecessary for IRIX
 * because of the streams monitor.
 *
 * XXXrs Will any brain-dead test actually try to call the put/service
 *	 procedure before/after calling qprocsoff?
 */
/* ARGSUSED */
void
qprocsoff(register queue_t *rq)
{
	return;
}

/*
 * DDI/DKI function to "freeze" all activity on a queue and set the
 * interrupt level to splstr.  The former part is unnecessary for IRIX
 * because of the streams monitor.
 */
/* ARGSUSED */
int	/* XXXrs should be pl_t? */
freezestr(queue_t *q)
{
	return(splstr());
}

/*
 * DDI/DKI function to "unfreeze" all activity on a queue and set the
 * interrupt level to s.  The former part is unnecessary for IRIX
 * because of the streams monitor.
 */
/* ARGSUSED */
void
unfreezestr(queue_t *q, int s) /* XXXrs s should be pl_t? */
{
	splx(s);
	return;
}

#ifdef _STRQ_TRACING
/*
 * Queue tracing code.
 */

int
__str_nchunks(mblk_t *bp)
{
	int i = 0;
	while (bp) {
		i++;
		bp = bp->b_cont;
	}
	return i;
}

void
q_trace_putq(queue_t *q, mblk_t *bp, inst_t *ra)
{
  ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_PUTQ,
	(void *)bp, (void *)(__psunsigned_t)__str_msgsize(bp), 
	(void *)(__psint_t)q->q_count, (void *)ra,
	(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
	(void *)curthreadp, (void *)__str_nchunks(bp), 0, 0, 0, 0, 0, 0, 0);
	return;
}

void
q_trace_insq(queue_t *q, mblk_t *bp, mblk_t *ep, inst_t *ra)
{
	ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_INSQ,
	(void *)bp, (void *)(__psunsigned_t)__str_msgsize(bp), 
	(void *)(__psint_t)q->q_count, (void *)ra,
	(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
	(void *)curthreadp, ep, (void *)__str_nchunks(bp), 0, 0, 0, 0, 0, 0);
	return;
}

void
q_trace_putbq(queue_t *q, mblk_t *bp, inst_t *ra)
{
	ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_PUTBQ,
	(void *)bp, (void *)(__psunsigned_t)__str_msgsize(bp), 
	(void *)(__psint_t)q->q_count, (void *)ra,
	(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
	(void *)curthreadp, (void *)__str_nchunks(bp), 0, 0, 0, 0, 0, 0, 0);
	return;
}

void
q_trace_getq(queue_t *q, mblk_t *bp, inst_t *ra)
{
	ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_GETQ,
	(void *)bp, (void *)(__psunsigned_t)__str_msgsize(bp), 
	(void *)(__psint_t)q->q_count, (void *)ra,
	(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
	(void *)curthreadp, (void *)__str_nchunks(bp), 0, 0, 0, 0, 0, 0, 0);
	return;
}

void
q_trace_rmvq(queue_t *q, mblk_t *bp, inst_t *ra)
{
	ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_RMVQ,
	(void *)bp, (void *)(__psunsigned_t)__str_msgsize(bp), 
	(void *)(__psint_t)q->q_count, (void *)ra,
	(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
	(void *)curthreadp, (void *)__str_nchunks(bp), 0, 0, 0, 0, 0, 0, 0);
	return;
}

void
q_trace_add_1(queue_t *q, inst_t *ra)
{
	ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_ADD1,
	(void *)0, (void *)0, 
	(void *)(__psint_t)q->q_count, (void *)ra,
	(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
	(void *)curthreadp, 0, 0, 0, 0, 0, 0, 0, 0);
	return;
}

void
q_trace_add_2(queue_t *q, inst_t *ra)
{
	ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_ADD2,
		(void *)0, (void *)0, 
		(void *)(__psint_t)q->q_count, (void *)ra,
		(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
		(void *)curthreadp, 0, 0, 0, 0, 0, 0, 0, 0);
	return;
}

void
q_trace_sub_1(queue_t *q, inst_t *ra)
{
	ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_SUB1,
		(void *)0, 0, 
		(void *)(__psint_t)q->q_count, (void *)ra,
		(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
		(void *)curthreadp, 0, 0, 0, 0, 0, 0, 0, 0);
	return;
}

void
q_trace_sub_2(queue_t *q, inst_t *ra)
{
	ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_SUB2,
		(void *)0, (void *)0, 
		(void *)(__psint_t)q->q_count, (void *)ra,
		(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
		(void *)curthreadp, 0, 0, 0, 0, 0, 0, 0, 0);
	return;
}

void
q_trace_zero(queue_t *q, inst_t *ra)
{
	ktrace_enter(q->q_trace, (void *)(__psint_t)QUEUE_KTRACE_ZERO,
		(void *)0, (void *)0, 
		(void *)(__psint_t)q->q_count, (void *)ra,
		(void *)(__psunsigned_t)q->q_flag, (void *)(__psint_t)cpuid(),
		(void *)curthreadp, 0, 0, 0, 0, 0, 0, 0, 0);
	return;
}
#endif /* _STRQ_TRACING */

