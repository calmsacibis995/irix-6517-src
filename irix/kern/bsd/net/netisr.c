/*
 * Routines to handle network input.
 *
 * Copyright 1997, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 3.75 $"

#include "tcp-param.h"
#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/param.h"
#include "sys/par.h"
#include "sys/atomic_ops.h"
#include "sys/runq.h"
#include "sys/debug.h"
#include "sys/sema.h"
#include "sys/errno.h"
#include "sys/systm.h"
#include "sys/siginfo.h"
#include "sys/mbuf.h"
#include "ksys/sthread.h"
#include "sys/schedctl.h"
#include "ksys/xthread.h"
#include "netisr.h"
#include "net/if.h"
#include "net/raw.h"
#include "net/route.h"
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/sockd.h>
#include <sys/rtmon.h>

/*
 * Map a CPU number into a "node" number, then map the node into
 * the CPU that handles input for that node. These logical nodes
 * need not correspond exactly to physical nodes, but the hope is
 * for some locality. More can be done later.
 */
#ifdef SN0
#define	NODE_NUMBER(cpu) cputocnode(cpu)
#else
#define NODE_NUMBER(cpu) (cpu / 2)
#endif

extern int netthread_pri;	/* sockd/netproc priority */
extern int netthread_float;	/* mustrun netproc unless true */
/*
 * This is the data structure for each network input process.
 */
struct per_netproc {
	struct ifqueue	netproc_q;	/* input queue */
	struct route	netproc_rt;	/* forwarding cache */
	thd_int_t	netproc_thread;	/* "interrupt" thread data */
} **netproc_data;

/*
 * The following "netproc" routines create and use a thread running at a
 * preemptable, nondegrading priority for packet processing.
 */

#ifdef _NET_UTRACE
#define NETIN_UTRACE(name, mb, ra) \
	UTRACE(RTMON_ALLOC, (name), (__int64_t)(mb), \
		   UTPACK((ra), (mb) ? (mb)->m_off : 0));
#else
#define NETIN_UTRACE(name, mb, ra)
#endif

#ifdef DEBUG
static struct {
	u_long  loop;
	u_long  sleep;		/* # times no packets pending, slept */
	u_long  intr;		/* # of interrupts */
	u_long  wake;		/* # times process was woken */
	u_long  none;		/* # times process wasn't waiting */
} nproc_stats;
#endif

static network_input_t *input_table[AF_MAX+1];

/*
 * Called once per address family to set up input function.
 * Just store it in the above table.
 */
void network_input_setup(int af, network_input_t func)
{
	if (af > AF_MAX)
	    cmn_err_tag(170,CE_PANIC, "address family %d out of range", af);
	input_table[af] = func;
}

/*
 * This is called from network interface device drivers when packets come in.
 * Use the direction policy wake up the right network input process.
 * Returns error code (zero is OK).
 * This is a critical performance path!
 */
int
network_input(struct mbuf *m, int af, int flags)
{
        int n = cpuid();
	struct ifqueue *ifq;
	int s;

	extern int max_netprocs;

	METER(nproc_stats.intr++);
	mtod(m, struct ifheader *)->ifh_af = af;
	ifq = &(netproc_data[n]->netproc_q);
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		NETIN_UTRACE(UTN('neti','drop'), m, __return_address);
		m_freem(m);
		return ENOBUFS;
	}
	NETIN_UTRACE(UTN('neti','que '), m, __return_address);
	IFQ_LOCK(ifq, s);
	IF_ENQUEUE_NOLOCK(ifq, m);
	IFQ_UNLOCK(ifq, s);
	if ((flags & NETPROC_MORETOCOME) == 0) {
		if (valusema(&(netproc_data[n]->netproc_thread.thd_isync)) >= 1)
		        cvsema(&(netproc_data[n]->netproc_thread.thd_isync));
		else
		        vsema(&(netproc_data[n]->netproc_thread.thd_isync));
	}
	return 0;
}


/*
 * System thread version of network input (per node).
 */
void
netproc(void *v)
{
	int n = (int)(__psunsigned_t)v;
	thd_int_t *tp = &(netproc_data[n]->netproc_thread);
	struct ifqueue *ifq;
	int s;
	int af;
	struct mbuf *m;
	struct route *rt;

	ifq = &(netproc_data[n]->netproc_q);
	rt = &(netproc_data[n]->netproc_rt);

	for (;;) {
		IFQ_LOCK(ifq, s);
		IF_DEQUEUE_NOLOCK(ifq, m);
		IFQ_UNLOCK(ifq, s);
		if (m == NULL) {
			METER(nproc_stats.sleep++);
			ipsema(&tp->thd_isync);
			/* NOTREACHED */
			continue;
		}
		NETIN_UTRACE(UTN('neti','dequ'), m, __return_address);
		METER(nproc_stats.loop++);
		af  = mtod(m, struct ifheader *) -> ifh_af;
		if (input_table[af]) {
			NETIN_UTRACE(UTN('neti','deli'), m, __return_address);
			(input_table[af])(m, rt);
		} else {
			NETIN_UTRACE(UTN('neti','noin'), m, __return_address);
			m_freem(m);
		}
	}
	/* NOTREACHED */
}

/*
 * Start-up of the network input threads
 */

void
netproc_init(void *v)
{
	cpuid_t cpu = (cpuid_t)(__psunsigned_t)v;
	thd_int_t *tp = &(netproc_data[cpu]->netproc_thread);
	extern int maxcpus;

	ASSERT_ALWAYS(cpu_enabled(cpu));
	if (netthread_float == 0) {
		(void) setmustrun(cpu);
	}

	xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *)netproc, 
		(void *)(__psunsigned_t)cpu);
	atomicSetInt(&tp->thd_flags, THD_INIT);
	ipsema(&tp->thd_isync);
	/* NOTREACHED */
}

int max_netprocs;

void
init_max_netprocs(void)
{
        /* Maybe we should do a compare_and_set() to make it atomic? */
	if (max_netprocs == 0)
	        max_netprocs = maxcpus;
}

void
netisr_init(void)
{
	int i;
	extern int numcpus;
	char thread_name[32];
	thd_int_t *tp;
	struct per_netproc *pnp;
	
	ASSERT(SN_MORETOCOME == NETPROC_MORETOCOME); /* assumed by ether.c */

	init_max_netprocs();
	netproc_data = (struct per_netproc **)kmem_zalloc(max_netprocs *  
		sizeof(struct per_netproc *), KM_NOSLEEP);
	if (netproc_data == NULL) {
		cmn_err_tag(171,CE_PANIC, "no memory for netproc");
	} 
	for (i=0; i<max_netprocs; i++) {
		/* allocate the thread state on the appropriate cpu */
	        if (!cpu_enabled(i)) {
#if DEBUG
		        cmn_err_tag(172,CE_WARN, "netproc_init: skipping cpu %d", i);
#endif
			continue;
		}
		pnp = (struct per_netproc *) kmem_zalloc_node(
			 sizeof (struct per_netproc), KM_NOSLEEP, NODE_NUMBER(i));
		if (pnp == NULL)
			cmn_err_tag(173,CE_PANIC, "no memory for netproc");
		netproc_data[i] = pnp;
		pnp->netproc_q.ifq_maxlen = 512;
		tp = &(pnp->netproc_thread);
		sprintf(thread_name, "netproc%d", i);
		atomicSetInt(&tp->thd_flags, THD_REG);

		/*
		 * After playing around with these priorities for a while,
		 * 192 doesn't seem to make latency noticably worse, and the
		 * overall system performance seems better than using 240 or
		 * 250.  We really need a deterministic set of tests to
		 * verify this.
		 */
		xthread_setup(thread_name, netthread_pri, tp, 
			      (xt_func_t *)netproc_init, 
			      (void *)(__psunsigned_t)i);
	}
}

/*
 * "sockd" is a mechanism to do all the periodic things
 * that need to be done with the BSD networking framework.
 */

#define	SOCKD_LOCK_INIT	mutex_init(&sockd_lock, MUTEX_DEFAULT, "sockd")
#define	SOCKD_LOCK(s)	mutex_lock(&sockd_lock, PZERO)
#define	SOCKD_UNLOCK(s)	mutex_unlock(&sockd_lock)
static mutex_t sockd_lock;
static struct sockd_entry *sockd_head, *sockd_tail, *sockd_free;
static zone_t *sockd_zone;
static void sockd_call(void (*)(), void *);
static void sockd(void);
#define SOCKD_FREEMAX	15
int sockd_freecnt = SOCKD_FREEMAX;
int sockd_freemax = SOCKD_FREEMAX;

static int pffasttimo_ticks;	/* ticks till pffasttimo */
static int pfslowtimo_ticks;	/* ticks till pfslowtimo */
static int if_slowtimo_ticks;	/* ticks till if_slowtimo */
static int lastwait;		/* length of last wait */

static int
periodic_timeouts(void)
{
	int waitticks, t;	/* number of ticks to wait to run again */

        waitticks = HZ;
        if (pftimo_active) {
                pffasttimo_ticks += lastwait;
                if (pffasttimo_ticks >= HZ/5) {         /* 200 ms */
                        pffasttimo();
                        pffasttimo_ticks = 0;
                        t = HZ/5;
                } else  {
                        t = HZ/5 - pffasttimo_ticks;
                }
                waitticks = t;

                pfslowtimo_ticks += lastwait;
                if (pfslowtimo_ticks >= HZ/2) {         /* 500 ms */
                        pfslowtimo();
                        pfslowtimo_ticks = 0;
                        t = HZ/2;
                } else {
                        t = HZ/2 - pfslowtimo_ticks;
                }
                if (t < waitticks)
                        waitticks = t;
        }
        if (if_slowtimo_active) {
                if_slowtimo_ticks += lastwait;
                if (if_slowtimo_ticks >= (IFNET_SLOWHZ * HZ)) { /* 1 sec */
                        if_slowtimo();
                        if_slowtimo_ticks = 0;
                }
        }
	lastwait = waitticks;
	return (waitticks);
}

#if (_MIPS_SZLONG == 32)
#define SOCKD_STACKSIZE 4096
#else
#define SOCKD_STACKSIZE 8192
#endif

void
sockd_init(void)
{
	register int i;

        SOCKD_LOCK_INIT;
        sockd_zone = kmem_zone_init(sizeof(struct sockd_entry), "sockd entry");
	/* create back up free list */
	for (i=sockd_freemax; i; i--) {
        	struct sockd_entry *entry;

		entry = kmem_zone_alloc(sockd_zone, KM_NOSLEEP);
		entry->next = sockd_free;
		sockd_free = entry;
	}
        sthread_create("sockd", NULL, SOCKD_STACKSIZE, 0, netthread_pri, KT_PS,
			(st_func_t *)sockd, 0, 0, 0, 0);
}

/*
 * sockd_timeout()
 */
int
sockd_timeout(void (*fun)(), void *arg, long tim)
{
        return (timeout(sockd_call, (void *)fun, tim, arg));
}

/*
 * sockd_call()
 */
static void
sockd_call(void (*fun)(), void *arg)
{
        struct sockd_entry *entry = kmem_zone_alloc(sockd_zone, KM_NOSLEEP);

	SOCKD_LOCK(s);

	if (!entry) {
		entry = sockd_free;
		if (!entry)
                	cmn_err_tag(174,CE_PANIC,
				"sockd_call: could not allocate sockd entry");
		else {
			sockd_free = entry->next;
			sockd_freecnt--;
		}
	}

        entry->func = fun;
        entry->arg1 = arg;
        entry->arg2 = NULL;
        entry->next = 0;
        if (!sockd_tail) {
                ASSERT(!sockd_head);
                sockd_head = sockd_tail = entry;
        } else {
                sockd_tail->next = entry;
                sockd_tail = entry;
        }
	SOCKD_UNLOCK(s);
}

/*
 * sockd()
 *
 * consolidates the handling of timeout events for networking;
 * many of the event handlers manipulate the same data structures
 * so they are done sequentially by sockd; periodic hardcoded
 * timeout events [pffasttimeo, pfslowtimo, and if_slowtimo] are
 * handled in a function which returns the number of ticks to the
 * next desired wakeup.  Others timeouts [ipfiltertimer and
 * nfs client side timeouts] are handled after; such events are
 * queued to be run by sockd when a timeout occurs by sockd_call.
 *
 * note: periodic timeouts will occur at least every 200ms,
 * a timeout queued to sockd may take up to that much longer
 * to happen.  If there are many events queued to sockd it
 * has the effect of lengthening periodic timeouts somewhat which
 * actually might be desired when the system is heavily loaded.
 */
static void
sockd(void)
{
	int ticks;
        struct sockd_entry *entry;

	while (1) {
		ticks = periodic_timeouts();

		SOCKD_LOCK(s);
		while (entry = sockd_head) {
			sockd_head = entry->next;
			if (sockd_head == 0) {
				ASSERT(entry == sockd_tail);
				sockd_tail = 0;
			}
			SOCKD_UNLOCK(s);

			entry->func(entry->arg1, entry->arg2);

			SOCKD_LOCK(s);
			if (sockd_freecnt < sockd_freemax) {
				entry->next = sockd_free;
				sockd_free = entry;
				sockd_freecnt++;
			} else {
				kmem_zone_free(sockd_zone, entry);
			}
		}
		SOCKD_UNLOCK(s);

		ASSERT(ticks >= 1);
		delay(ticks);
	}
}
