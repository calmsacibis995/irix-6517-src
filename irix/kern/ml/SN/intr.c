/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * intr.c-
 *	This file contains all of the routines necessary to set up and
 *	handle interrupts on an IP27 board.
 */

#ident  "$Revision: 1.150 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reg.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/pda.h>
#include <sys/sysinfo.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/numa.h>
#include <sys/SN/agent.h>
#include <sys/SN/intr.h>

#if defined (SN0)
#include <sys/SN/SN0/IP27.h>
#endif /* SN0 */

#if defined (SN1)
#include <sys/SN/SN1/IP33.h>
#endif /* SN0 */

#include "sn_private.h"
#include <sys/runq.h>
#include <sys/ktime.h>
#include <ksys/xthread.h>
#include <sys/schedctl.h>		/* For thread priority defs */
#include <sys/atomic_ops.h>
#include <sys/idbgentry.h>
#if DEBUG_INTR_TSTAMP_DEBUG
#include <sys/debug.h>
#include <sys/idbg.h>
#include <sys/inst.h>
void do_splx_log(int, int);
void spldebug_log_event(int);
#endif

extern int	nmied;
extern int	hub_intr_wakeup_cnt;

/*
 * We pass the ep as the second argument for those stacked intr handlers
 * that may need it.  This hack will get revisited later.
 */
typedef void (*intr_ep_func_t)(intr_arg_t, eframe_t *);
#define INTR_CALL_HANDLER(_vec, _lvl, _ep) \
	(*((intr_ep_func_t)((_vec)[_lvl]).iv_func))((_vec)[_lvl].iv_arg, _ep)

#define INTR_CALL_PROLOGUE_HANDLER(_vec, _lvl, _ep) { \
if ((_vec)[_lvl].iv_prefunc) \
    (*((intr_ep_func_t)((_vec)[_lvl]).iv_prefunc))((_vec)[_lvl].iv_arg, _ep); \
}

#ifdef DEBUG
#define INTPEND0_ITHREAD_QUALIFY(_vec, _lvl) \
        ithread_qualify( (void*)(((_vec)[_lvl]).iv_func) )
#else
#define INTPEND0_ITHREAD_QUALIFY(_vec, _lvl)
#endif /* DEBUG */

#define INTR_LOCK(vecblk) \
     (s = mutex_spinlock_spl(&(vecblk)->vector_lock, (vecblk)->vector_spl))
#define INTR_UNLOCK(vecblk) \
      mutex_spinunlock(&(vecblk)->vector_lock, s)

/*
 * REACT/Pro
 */
#include <sys/rtmon.h>

extern void early_counter_intr(eframe_t *);
extern void timein(struct eframe_s *);

static void intr_thread_setup(intr_vector_t *, cnodeid_t cnode, int bit,ilvl_t intr_swlevel);

/* XXX - There should be one of these per node! */
/*
 * Table of cause register interrupt handlers:
 * 	initialize these to NULL so that we can tell when we take an interrupt
 *	too early.
 */

static void intpend0(eframe_t *ep);
static void intpend1(eframe_t *ep);

struct cause_intr_s {
	intr_func_t	handler;
	uint		intr_spl_mask;
	int		check_kp;
} cause_intr_tbl[NUM_CAUSE_INTRS] = {
	{ (intr_func_t)timein, SR_HI_MASK|SR_IE, 1 },	/* SRB_SWTIMO	*/
	{ (intr_func_t)NULL, 0, 1 },			/* SRB_NET	*/
	{ (intr_func_t)intpend0, SR_HI_MASK|SR_IE, 1 },	/* SRB_DEV0	*/
	{ (intr_func_t)intpend1, SR_ALL_MASK|SR_IE, 0 },/* SRB_DEV1	*/
	{ (intr_func_t)timein, SR_HI_MASK|SR_IE, 1 },	/* SRB_TIMOCLK	*/
	{ (intr_func_t)NULL, 0, 0 },			/* SRB_PROFCLK	*/
	{ (intr_func_t)NULL, 0, 0 },			/* SRB_ERR	*/
	{ (intr_func_t)early_counter_intr, SR_HI_MASK|SR_IE, 1 }/* SRB_SCHEDCLK */
};
	
/* XXX - temporary */
intr_func_t sched_intr = (intr_func_t)early_counter_intr;

/* 
 * Find first bit set 
 * Used outside this file also 
 */
int ms1bit(unsigned long x)
{
    int			b;

    if (x >> 32)	b  = 32, x >>= 32;
    else		b  =  0;
    if (x >> 16)	b += 16, x >>= 16;
    if (x >>  8)	b +=  8, x >>=  8;
    if (x >>  4)	b +=  4, x >>=  4;
    if (x >>  2)	b +=  2, x >>=  2;

    return b + (int) (x >> 1);
}

/* ARGSUSED */
void
intr_stray(void *lvl)
{
    cmn_err(CE_WARN,"Stray Interrupt - level %d to cpu %d", lvl, cpuid());
}

#if defined(DEBUG)

/* Infrastructure  to gather the device - target cpu mapping info */
#define MAX_DEVICES	1000	/* Reasonable large number . Need not be 
				 * the exact maximum # devices possible.
				 */
#define MAX_NAME	100	
typedef struct {
	dev_t		dev;	/* device */
	cpuid_t		cpuid;	/* target cpu */
	cnodeid_t	cnodeid;/* node on which the target cpu is present */
	int		bit;	/* intr bit reserved */
	char		intr_name[MAX_NAME]; /* name of the interrupt */
} intr_dev_targ_map_t;

intr_dev_targ_map_t 	intr_dev_targ_map[MAX_DEVICES];
__uint64_t		intr_dev_targ_map_size;
lock_t			intr_dev_targ_map_lock;

/* Print out the device - target cpu mapping.
 * This routine is used only in the idbg command
 * "intrmap" 
 */
void
intr_dev_targ_map_print(cnodeid_t cnodeid)
{
	int  i,size = 0;
	int  print_flag = 0,verbose = 0;	
	char node_name[10];
	
	if (cnodeid != CNODEID_NONE) {
		nodepda_t 	*npda;

		npda = NODEPDA(cnodeid);
		qprintf("\n INT_PEND0: ");
		for(i = 0 ; i < N_INTPEND_BITS ; i++)
			qprintf("%d",npda->intr_dispatch0.info[i].ii_flags);
		qprintf("\n INT_PEND1: ");
		for(i = 0 ; i < N_INTPEND_BITS ; i++)
			qprintf("%d",npda->intr_dispatch1.info[i].ii_flags);
		verbose = 1;
	}
	sprintf(node_name,": %d",cnodeid);
	qprintf("\n Device - Target Map [Interrupts: %s Node%s]\n\n",
		(verbose ? "All" : "Non-hardwired"),
		(cnodeid == CNODEID_NONE) ? "s: All" : node_name); 
		
	qprintf("Device\tCpu\tCnode\tIntr_bit\tIntr_name\n");
	for (i = 0 ; i < intr_dev_targ_map_size ; i++) {

		print_flag = 0;
		if (verbose) {
			if (cnodeid != CNODEID_NONE) {
				if (cnodeid == intr_dev_targ_map[i].cnodeid)
					print_flag = 1;
			} else {
				print_flag = 1;
			}
		} else {
			if (intr_dev_targ_map[i].dev != 0) {
				if (cnodeid != CNODEID_NONE) {
					if (cnodeid == 
					    intr_dev_targ_map[i].cnodeid)
						print_flag = 1;
				} else {
					print_flag = 1;
				}
			}
		}
		if (print_flag) {
			size++;
			qprintf("%d\t%d\t%d\t%d\t%s\n",
				intr_dev_targ_map[i].dev,
				intr_dev_targ_map[i].cpuid,
				intr_dev_targ_map[i].cnodeid,
				intr_dev_targ_map[i].bit,
				intr_dev_targ_map[i].intr_name);
		}

	}
	qprintf("\nTotal : %d\n",size);
}
#endif /* DEBUG */

/*
 * The spinlocks have already been initialized.  Now initialize the interrupt
 * vectors.  One processor on each hub does the work.
 */
void
intr_init(void)
{
    int s;
    hub_intmasks_t *intmasks;
    intr_vecblk_t *vecblk;
    int ip = 0;
    int i;

    do {
	if (ip == 0) {
	    /* Set up INT_PEND0 */
	    intmasks = &private.p_intmasks;
	    vecblk = intmasks->dispatch0;
	} else {
	    /* Set up INT_PEND1 */
	    intmasks = &private.p_intmasks;
	    vecblk = intmasks->dispatch1;
	}

	ASSERT(vecblk);

	INTR_LOCK(vecblk);

	/* The structure may be initialized by either CPU on a Hub. */
	if (vecblk->vector_state == VECTOR_UNINITED) {
	    /* Initialize this hubs vector. */
	    for (i = 0; i < N_INTPEND_BITS; i++) {
		vecblk->vectors[i].iv_func = intr_stray;
		vecblk->vectors[i].iv_prefunc = NULL;
		vecblk->vectors[i].iv_arg = (void *)(__psint_t)(ip * N_INTPEND_BITS + i);

		vecblk->info[i].ii_owner_dev = 0;
		strcpy(vecblk->info[i].ii_name, "Unused");
		vecblk->info[i].ii_flags = 0;	/* No flags */
		vecblk->vectors[i].iv_tinfo.thd_flags = 0; /* Not threaded yet. */
		vecblk->vectors[i].iv_mustruncpu = -1; /* No CPU yet. */
	    }

	    /* initialize the counts */
	    vecblk->vector_count = 0;    
	    for (i = 0; i < CPUS_PER_NODE; i++)
		    vecblk->cpu_count[i] = 0;

	    /* Done. */
	    vecblk->vector_state = VECTOR_INITED;
	}

	INTR_UNLOCK(vecblk);

    } while (++ip < 2);

    return;
}
/*
 * do_intr_reserve_level(cpuid_t cpu, int bit, int resflags, int reserve, 
 *					vertex_hdl_t owner_dev, char *name)
 *	Internal work routine to reserve or unreserve an interrupt level.
 *		cpu is the CPU to which the interrupt will be sent.
 *		bit is the level bit to reserve.  -1 means any level
 *		resflags should include II_ERRORINT if this is an
 *			error interrupt, II_THREADED if the interrupt handler
 *			will be threaded, or 0 otherwise.
 *		reserve should be set to II_RESERVE or II_UNRESERVE
 *			to get or clear a reservation.
 *		owner_dev is the device that "owns" this interrupt, if supplied
 *		name is a human-readable name for this interrupt, if supplied
 *	intr_reserve_level returns the bit reserved or -1 to indicate an error
 */
static int
do_intr_reserve_level(cpuid_t cpu, int bit, int resflags, int reserve, 
					vertex_hdl_t owner_dev, char *name)
{
    intr_vecblk_t	*vecblk;
    hub_intmasks_t 	*hub_intmasks;
    int s;
    int rv = 0;
    int ip;

#ifdef SABLE
    ASSERT_ALWAYS(bit < N_INTPEND_BITS * 2);
#else
    ASSERT(bit < N_INTPEND_BITS * 2);
#endif

    hub_intmasks = &pdaindr[cpu].pda->p_intmasks;

    if (pdaindr[cpu].pda == NULL) return -1;
    if ((bit < N_INTPEND_BITS) && !(resflags & II_ERRORINT)) {
	vecblk = hub_intmasks->dispatch0;
	ip = 0;
    } else {
	ASSERT((bit >= N_INTPEND_BITS) || (bit == -1));
	bit -= N_INTPEND_BITS;	/* Get position relative to INT_PEND1 reg. */
	vecblk = hub_intmasks->dispatch1;
	ip = 1;
    }

    INTR_LOCK(vecblk);

    if (bit <= -1) {
	bit = 0;
#ifdef SABLE
	ASSERT_ALWAYS(reserve == II_RESERVE);
#else
	ASSERT(reserve == II_RESERVE);
#endif
	/* Choose any available level */
	for (; bit < N_INTPEND_BITS; bit++) {
	    if (!(vecblk->info[bit].ii_flags & II_RESERVE)) {
		rv = bit;
		break;
	    }
	}

	/* Return -1 if all interrupt levels int this register are taken. */
	if (bit == N_INTPEND_BITS)
	    rv = -1;

    } else {
	/* Reserve a particular level if it's available. */
	if ((vecblk->info[bit].ii_flags & II_RESERVE) == reserve) {
	    /* Can't (un)reserve a level that's already (un)reserved. */
	    rv = -1;
	} else {
	    rv = bit;
	}
    }

    /* Reserve the level and bump the count. */
    if (rv != -1) {
	if (reserve) {
	    int maxlen = sizeof(vecblk->info[bit].ii_name) - 1;
	    int namelen;
	    vecblk->info[bit].ii_flags |= (II_RESERVE | resflags);
	    vecblk->info[bit].ii_owner_dev = owner_dev;
	    /* Copy in the name. */
	    namelen = name ? strlen(name) : 0;
	    strncpy(vecblk->info[bit].ii_name, name, MIN(namelen, maxlen)); 
	    vecblk->info[bit].ii_name[maxlen] = '\0';
	    vecblk->vector_count++;
	    if (resflags & II_THREADED)
		vecblk->vectors[bit].iv_tinfo.thd_flags |= THD_ISTHREAD;
	    else
		vecblk->vectors[bit].iv_tinfo.thd_flags &= ~THD_ISTHREAD;
	} else {
	    vecblk->info[bit].ii_flags = 0;	/* Clear all the flags */
	    vecblk->info[bit].ii_owner_dev = 0;
	    /* Clear the name. */
	    vecblk->info[bit].ii_name[0] = '\0';
	    vecblk->vector_count--;
	}
    }

    INTR_UNLOCK(vecblk);

#if defined(DEBUG)
    if (rv >= 0) {
	    int namelen = name ? strlen(name) : 0;
	    /* Gather this device - target cpu mapping information
	     * in a table which can be used later by the idbg "intrmap"
	     * command
	     */
	    s = mutex_spinlock(&intr_dev_targ_map_lock);
	    if (intr_dev_targ_map_size < MAX_DEVICES) {
		    intr_dev_targ_map_t	*p;

		    p 		= &intr_dev_targ_map[intr_dev_targ_map_size];
		    p->dev  	= owner_dev;
		    p->cpuid 	= cpu; 
		    p->cnodeid 	= cputocnode(cpu); 
		    p->bit 	= ip * N_INTPEND_BITS + rv;
		    strncpy(p->intr_name,
			    name,
			    MIN(MAX_NAME,namelen));
		    intr_dev_targ_map_size++;
	    }
	    mutex_spinunlock(&intr_dev_targ_map_lock,s);
    }
#endif /* DEBUG */

    return (((rv == -1) ? rv : (ip * N_INTPEND_BITS) + rv)) ;
}


/*
 * WARNING:  This routine should only be called from within ml/SN0.
 *	Reserve an interrupt level.
 */
int
intr_reserve_level(cpuid_t cpu, int bit, int resflags, vertex_hdl_t owner_dev, char *name)
{
	return(do_intr_reserve_level(cpu, bit, resflags, II_RESERVE, owner_dev, name));
}


/*
 * WARNING:  This routine should only be called from within ml/SN0.
 *	Unreserve an interrupt level.
 */
void
intr_unreserve_level(cpuid_t cpu, int bit)
{
	(void)do_intr_reserve_level(cpu, bit, 0, II_UNRESERVE, 0, NULL);
}

/*
 * Get values that vary depending on which CPU and bit we're operating on
 */
static hub_intmasks_t *
intr_get_ptrs(cpuid_t cpu, int bit,
	      int *new_bit,		/* Bit relative to the register */
	      hubreg_t **intpend_masks, /* Masks for this register */
	      intr_vecblk_t **vecblk,	/* Vecblock for this interrupt */
	      int *ip)			/* Which intpend register */
{
	hub_intmasks_t *hub_intmasks;

	ASSERT(bit < N_INTPEND_BITS * 2);
	hub_intmasks = &pdaindr[cpu].pda->p_intmasks;

	if (bit < N_INTPEND_BITS) {
		*intpend_masks = hub_intmasks->intpend0_masks;
		*vecblk = hub_intmasks->dispatch0;
		*ip = 0;
		*new_bit = bit;
	} else {
		*intpend_masks = hub_intmasks->intpend1_masks;
		*vecblk = hub_intmasks->dispatch1;
		*ip = 1;
		*new_bit = bit - N_INTPEND_BITS;
	}

	return hub_intmasks;
}


/*
 * intr_connect_level(cpuid_t cpu, int bit, ilvl_t intr_swlevel, 
 *		intr_func_t intr_func, void *intr_arg);
 *	This is the lowest-level interface to the interrupt code.  It shouldn't
 *	be called from outside the ml/SN0 directory.
 *	intr_connect_level hooks up an interrupt to a particular bit in
 *	the INT_PEND0/1 masks.  Returns 0 on success.
 *		cpu is the CPU to which the interrupt will be sent.
 *		bit is the level bit to connect to
 *		intr_swlevel tells which software level to use
 *		intr_func is the interrupt handler
 *		intr_arg is an arbitrary argument interpreted by the handler
 *		intr_prefunc is a prologue function, to be called
 *			with interrupts disabled, to disable
 *			the interrupt at source.  It is called
 *			with the same argument.  Should be NULL for
 *			typical interrupts, which can be masked
 *			by the infrastructure at the level bit.
 *	intr_connect_level returns 0 on success or nonzero on an error
 */
/* ARGSUSED */
int
intr_connect_level(cpuid_t cpu, int bit, ilvl_t intr_swlevel, 
		intr_func_t intr_func, void *intr_arg,
		intr_func_t intr_prefunc)
{
    intr_vecblk_t	*vecblk;
    hubreg_t		*intpend_masks;
    int s;
    int rv = 0;
    int ip;

    ASSERT(cpu_enabled(cpu));

#ifdef SABLE
    ASSERT_ALWAYS(bit < N_INTPEND_BITS * 2);
#else
    ASSERT(bit < N_INTPEND_BITS * 2);
#endif

    (void)intr_get_ptrs(cpu, bit, &bit, &intpend_masks,
				 &vecblk, &ip);

    INTR_LOCK(vecblk);

    if ((vecblk->info[bit].ii_flags & II_INUSE) ||
	(!(vecblk->info[bit].ii_flags & II_RESERVE))) {
	/* Can't assign to a level that's in use or isn't reserved. */
	rv = -1;
    } else {
	/* Stuff parameters into vector and info */
	vecblk->vectors[bit].iv_func = intr_func;
	vecblk->vectors[bit].iv_prefunc = intr_prefunc;
	vecblk->vectors[bit].iv_arg = intr_arg;
	vecblk->vectors[bit].iv_pri = intr_swlevel;
	vecblk->info[bit].ii_flags |= II_INUSE;
    }

    /* Now stuff the masks if everything's okay. */
    if (!rv) {
	int slice;
	volatile hubreg_t *mask_reg;
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cputocnode(cpu));

	/* Make sure it's not already pending when we connect it. */
	REMOTE_HUB_CLR_INTR(nasid, bit + ip * N_INTPEND_BITS);

	intpend_masks[0] |= (1ULL << (__uint64_t)bit);

	slice = cputoslice(cpu);
	vecblk->cpu_count[slice]++;
	if (ip == 0) {
		mask_reg = REMOTE_HUB_ADDR(nasid, 
		        PI_INT_MASK0_A + PI_INT_MASK_OFFSET * slice);
	} else {
		mask_reg = REMOTE_HUB_ADDR(nasid, 
			PI_INT_MASK1_A + PI_INT_MASK_OFFSET * slice);
	}

	/*
	 * If we're threaded handler, set up the thread here.
	 * If !ithreads_enabled, then it's early, and enable_ithreads()
	 * will come by and setup the thread for us.
	 */
/* XXX - FIXME - Move this flag elsewhere!  It's blowing our cache blocking. */
	if (vecblk->vectors[bit].iv_tinfo.thd_flags & THD_ISTHREAD) {
		/* Only INT_PEND0 interrupts can be threaded. */
		ASSERT(ip == 0);
		atomicSetInt(&vecblk->vectors[bit].iv_tinfo.thd_flags, THD_REG);
	        ASSERT(cpu_enabled(cpu));
		vecblk->vectors[bit].iv_mustruncpu = cpu;
		if (vecblk->ithreads_enabled)
			intr_thread_setup(&vecblk->vectors[bit],
					  cputocnode(cpu), bit,intr_swlevel);
	}

	HUB_S(mask_reg, intpend_masks[0]);
    }

    INTR_UNLOCK(vecblk);

    return rv;
}


/*
 * intr_disconnect_level(cpuid_t cpu, int bit)
 *
 *	This is the lowest-level interface to the interrupt code.  It should
 *	not be called from outside the ml/SN0 directory.
 *	intr_disconnect_level removes a particular bit from an interrupt in
 * 	the INT_PEND0/1 masks.  Returns 0 on success or nonzero on failure.
 */
int
intr_disconnect_level(cpuid_t cpu, int bit)
{
    intr_vecblk_t	*vecblk;
    hubreg_t		*intpend_masks;
    int s;
    int rv = 0;
    int ip;

    (void)intr_get_ptrs(cpu, bit, &bit, &intpend_masks,
				 &vecblk, &ip);

    INTR_LOCK(vecblk);

    if ((vecblk->info[bit].ii_flags & (II_RESERVE | II_INUSE)) !=
	((II_RESERVE | II_INUSE))) {
	/* Can't remove a level that's not in use or isn't reserved. */
	rv = -1;
    } else {
	/* Stuff parameters into vector and info */
	vecblk->vectors[bit].iv_func = (intr_func_t)NULL;
	vecblk->vectors[bit].iv_prefunc = (intr_func_t)NULL;
	vecblk->vectors[bit].iv_arg = 0;
	vecblk->info[bit].ii_flags &= ~II_INUSE;
#ifdef BASE_ITHRTEAD
	vecblk->vectors[bit].iv_mustruncpu = -1; /* No mustrun CPU any more. */
#endif
    }

    /* Now clear the masks if everything's okay. */
    if (!rv) {
	int slice;
	volatile hubreg_t *mask_reg;

	intpend_masks[0] &= ~(1ULL << (__uint64_t)bit);
	slice = cputoslice(cpu);
	vecblk->cpu_count[slice]--;
	mask_reg = REMOTE_HUB_ADDR(COMPACT_TO_NASID_NODEID(cputocnode(cpu)), 
				   ip == 0 ? PI_INT_MASK0_A : PI_INT_MASK1_A);
	mask_reg = (volatile hubreg_t *)((__psunsigned_t)mask_reg +
					(PI_INT_MASK_OFFSET * slice));
	*mask_reg = intpend_masks[0];
    }

    INTR_UNLOCK(vecblk);

    return rv;
}

/*
 * Actually block or unblock an interrupt
 */
void
do_intr_block_bit(cpuid_t cpu, int bit, int block)
{
	intr_vecblk_t *vecblk;
	int s;
	int ip;
	hubreg_t *intpend_masks;
	volatile hubreg_t mask_value;
	volatile hubreg_t *mask_reg;

	intr_get_ptrs(cpu, bit, &bit, &intpend_masks, &vecblk, &ip);

	INTR_LOCK(vecblk);

	if (block)
		/* Block */
		intpend_masks[0] &= ~(1ULL << (__uint64_t)bit);
	else
		/* Unblock */
		intpend_masks[0] |= (1ULL << (__uint64_t)bit);

	if (ip == 0) {
		mask_reg = REMOTE_HUB_ADDR(COMPACT_TO_NASID_NODEID(cputocnode(cpu)), 
		        PI_INT_MASK0_A);
	} else {
		mask_reg = REMOTE_HUB_ADDR(COMPACT_TO_NASID_NODEID(cputocnode(cpu)), 
			PI_INT_MASK1_A);
	}

	HUB_S(mask_reg, intpend_masks[0]);

	/*
	 * Wait for it to take effect.  (One read should suffice.)
	 * This is only necessary when blocking an interrupt
	 */
	if (block)
		while (mask_value = HUB_L(mask_reg) != intpend_masks[0])
			;

	INTR_UNLOCK(vecblk);
}


/*
 * Block a particular interrupt (cpu/bit pair).
 */
/* ARGSUSED */
void
intr_block_bit(cpuid_t cpu, int bit)
{
	do_intr_block_bit(cpu, bit, 1);
}


/*
 * Unblock a particular interrupt (cpu/bit pair).
 */
/* ARGSUSED */
void
intr_unblock_bit(cpuid_t cpu, int bit)
{
	do_intr_block_bit(cpu, bit, 0);
}


/*
 * Choose one of the CPUs on a specified node to receive interrupts.
 * Don't pick a cpu which has been specified as a NOINTR cpu.
 */
/* ARGSUSED */
static cpuid_t
intr_cpu_choose(cnodeid_t cnode)
{
	intr_vecblk_t *vecblk = &NODEPDA(cnode)->intr_dispatch0;
	cpuid_t cpu_a, cpu_b;
	int cpu_a_valid, cpu_b_valid;	
	/*	
	 * XXX - The assignment for error interrupts will be a bit
	 * weird.  We're only taking INTPEND0 into account.  We can
	 * fix this later if we want.
	 */

	cpu_a = cnode_slice_to_cpuid(cnode, 0);
	cpu_b = cnode_slice_to_cpuid(cnode, 1);
	
	/* Remember if cpu "a" can be chosen as an interrupt target */
	cpu_a_valid = cpu_enabled(cpu_a) && cpu_allows_intr(cpu_a);
	/* Remember if cpu "b" can be chosen as an interrupt target */
	cpu_b_valid = cpu_enabled(cpu_b) && cpu_allows_intr(cpu_b);

	/* If EXACTLY one of the cpus can be a possible interrupt target
	 * choose that 
	 */
	if (!cpu_b_valid && cpu_a_valid)
		return(cpu_a);

	if (!cpu_a_valid && cpu_b_valid)
		return(cpu_b);
	
	/* If NEITHER cpu can be a possible interrupt target 
	 * then we cannot choose a cpu on this node
	 */
	if (!cpu_a_valid && !cpu_b_valid)
		return(CPU_NONE);
	
	/* At this point we know that both the cpus are viable
	 * interrupt target candidates.
	 * Choose the cpu with the lowest load of interrupts.
	 */
	if (vecblk->cpu_count[0] < vecblk->cpu_count[1]) {
		return(cpu_a);
	} else
		return(cpu_b);

}


/*
 * Given a device descriptor, extract interrupt target information and
 * choose an appropriate CPU.  Return CPU_NONE if we can't make sense
 * out of the target information.
 * TBD: Should this be considered platform-independent code?
 */
static cpuid_t
intr_target_from_desc(device_desc_t dev_desc)
{
	cpuid_t cpuid = CPU_NONE;
	cnodeid_t cnodeid;
	vertex_hdl_t intr_target_dev;

	if ((intr_target_dev = device_desc_intr_target_get(dev_desc)) != GRAPH_VERTEX_NONE) {
		/* 
		 * A valid device was specified.  If it's a particular
		 * CPU, then use that CPU as target.  
		 */
		cpuid = cpuvertex_to_cpuid(intr_target_dev);
		if (cpuid != CPU_NONE)
			return(cpuid);

		/*
		 * Otherwise, pick a CPU on the node that owns the 
		 * specified target.
		 */
		cnodeid = master_node_get(intr_target_dev);
		if (cnodeid != CNODEID_NONE)
			cpuid = intr_cpu_choose(cnodeid);
	}

	return(cpuid);
}


/*
 * Check if we had already visited this candidate cnode
 */
static void *
intr_cnode_seen(cnodeid_t 	candidate,
		void 		*arg1,
		void 		*arg2)
{
	int		i;
	cnodeid_t	*visited_cnodes = (cnodeid_t *)arg1;
	int		*num_visited_cnodes = (int *)arg2;

	ASSERT(visited_cnodes);
	ASSERT(*num_visited_cnodes <= numnodes);
	for(i = 0 ; i < *num_visited_cnodes; i++) {
		if (candidate == visited_cnodes[i])
			return(NULL);
	}
	return(visited_cnodes);
}

/*
 * intr_bit_reserve_test(cpuid,cnode,req_bit,intr_resflags,owner_dev,intr_name,
 *			 *resp_bit)
 *		Either cpuid is not CPU_NONE or cnodeid not CNODE_NONE but
 * 		not both.
 * 1. 	If cpuid is not CPU_NONE this routine tests if this cpu can be a valid
 * 	interrupt target candidate.
 * 2. 	If cnodeid is not CNODE_NONE this routine tests if there is a cpu on 
 *	this node which can be a valid interrupt target candidate.
 * 3.	If a valid interrupt target cpu candidate is found then an attempt at 
 * 	reserving an interrupt bit on the corresponding cnode is made.
 *	
 * If steps 1 & 2 both fail or step 3 fails then we are not able to get a valid
 * interrupt target cpu then routine returns CPU_NONE (failure)
 * Otherwise routine returns cpuid of interrupt target (success)
 */
static cpuid_t
intr_bit_reserve_test(cpuid_t 		cpuid,
		      cnodeid_t 	cnodeid,
		      int		req_bit,
		      int 		intr_resflags,
		      dev_t 		owner_dev,
		      char		*intr_name,
		      int		*resp_bit)
{
	if (cnodeid != CNODEID_NONE) {
		/* Try to choose a interrupt cpu candidate */
		cpuid = intr_cpu_choose(cnodeid);
	}

	if (cpuid != CPU_NONE) {
		/* Try to reserve an interrupt bit on the hub 
		 * corresponding to the canidate cnode. If we
		 * are successful then we got a cpu which can
		 * act as an interrupt target for the io device.
		 * Otherwise we need to continue the search
		 * further.
		 */
		*resp_bit = do_intr_reserve_level(cpuid, 
						  req_bit,
						  intr_resflags,
						  II_RESERVE,
						  owner_dev, 
						  intr_name);

		if (*resp_bit >= 0)
			/* The interrupt target  specified was fine */
			return(cpuid);
	}
	return(CPU_NONE);
}
/*
 * intr_heuristic(dev_t dev,device_desc_t dev_desc,
 *		  int req_bit,int intr_resflags,dev_t owner_dev,
 *		  char *intr_name,int *resp_bit)
 *
 * Choose an interrupt destination for an interrupt.
 *	dev is the device for which the interrupt is being set up
 *	intr_desc is a description of hardware and policy that could
 *		help determine where this interrupt should go
 *
 *	req_bit is the interrupt bit requested 
 *		(can be INTRCONNECT_ANY_BIT in which the first available
 * 		 interrupt bit is used)
 *	intr_resflags indicates whether we want to (un)reserve bit
 *	owner_dev is the owner device
 *	intr_name is the readable interrupt name	
 * 	resp_bit indicates whether we succeeded in getting the required
 *		 action  { (un)reservation} done	
 *		 negative value indicates failure
 *
 */
/* ARGSUSED */
cpuid_t
intr_heuristic(dev_t 		dev,
	       device_desc_t 	dev_desc,
	       int		req_bit,
	       int 		intr_resflags,
	       dev_t 		owner_dev,
	       char		*intr_name,
	       int		*resp_bit)
{
	cpuid_t		cpuid;				/* possible intr targ*/
	cnodeid_t	visited_cnodes[MAX_NASIDS], 	/* nodes seen so far */
		        center,				/* node we are on */
		        candidate;			/* possible canidate */
	int		num_visited_cnodes = 0;		/* # nodes seen */
	int		radius = 1,			/* start looking at the
							 * current node
							 */
		        maxradius = physmem_maxradius();
	void		*rv;

	/* 
	 * If an interrupt target was specified for this
	 * interrupt allocation, use it.
	 */
	if (dev_desc) {

		/* Try to see if the interrupt target specified in the
		 * device descriptor a legal candidate.
		 */
		cpuid = intr_bit_reserve_test(intr_target_from_desc(dev_desc),
					      CNODEID_NONE,
					      req_bit,
					      intr_resflags,
					      owner_dev,
					      intr_name,
					      resp_bit);
		if (cpuid != CPU_NONE) 
			return(cpuid);	/* got a valid interrupt target */
		/* Fall through on to the next step in the search for
		 * the interrupt candidate.
		 */

	}
	
	/* Check if we can find a valid interrupt target candidate on
	 * the master node for the device.
	 */
	cpuid = intr_bit_reserve_test(CPU_NONE,
				      master_node_get(dev),
				      req_bit,
				      intr_resflags,
				      owner_dev,
				      intr_name,
				      resp_bit);

	if (cpuid != CPU_NONE)
		return(cpuid);	/* got a valid interrupt target */
	/* Fall through into the default algorithm
	 * (exhaustive-search-for-the-nearest-possible-interrupt-target)
	 * for finding the interrupt target
	 */

	/* 
	 * No valid interrupt specification exists.
	 * Try to find a node which is closest to the current node
	 * which can process interrupts from a device
	 */

	center = cnodeid();
	while (radius <= maxradius) {

		/* Try to find a node at the given radius and which
		 * we haven't seen already.
		 */
		rv = physmem_select_neighbor_node(center,radius,&candidate,
						  intr_cnode_seen,
						  (void *)visited_cnodes,
						  (void *)&num_visited_cnodes);
		if (!rv) {
			/* We have seen all the nodes at this particular radius
			 * Go on to the next radius level.
			 */
			radius++;
			continue;
		}			      
		/* We are seeing this candidate  cnode for the first time
		 */
		visited_cnodes[num_visited_cnodes++] = candidate;

		cpuid = intr_bit_reserve_test(CPU_NONE,
					      candidate,
					      req_bit,
					      intr_resflags,
					      owner_dev,
					      intr_name,
					      resp_bit);

		if (cpuid != CPU_NONE) 
			return(cpuid);	/* got a valid interrupt target */
	}
	/* In the worst case try to allocate interrupt bits on the
	 * master processor's node. We may get here during error interrupt
	 * allocation phase when the topology matrix is not yet setup
	 * and hence cannot do an exhaustive search.
	 */
	ASSERT(cpu_allows_intr(master_procid));
	cpuid = intr_bit_reserve_test(master_procid,
				      CNODEID_NONE,
				      req_bit,
				      intr_resflags,
				      owner_dev,
				      intr_name,
				      resp_bit);
	if (cpuid != CPU_NONE)
		return(cpuid);
	return(CPU_NONE);	/* Should never get here */
}


/*
 * cause_intr_connect
 *	Connect an interrupt to R10k cause bit "level".
 */
int
cause_intr_connect(int level, intr_func_t handler, uint intr_spl_mask)
{
	/*
	 * Make sure the request is in range and isn't for one of the hard-
	 * coded interrupt handlers (SRB_DEV0_IDX, SRB_DEV1_IDX)
	 */
	if ((level < 0) || (level >= NUM_CAUSE_INTRS) ||
	    (level == SRB_DEV0_IDX) || (level == SRB_DEV1_IDX)) {
		return -1;
	} else {
		cause_intr_tbl[level].handler = handler;
		cause_intr_tbl[level].intr_spl_mask = intr_spl_mask;
	}
	return 0;
}


/*
 * cause_intr_disconnect
 *	Disconnect an interrupt from R10k cause bit "level".
 */
int
cause_intr_disconnect(int level) {
	/* Make sure the level is in range. */
	if ((level < 0) || (level >= NUM_CAUSE_INTRS) ||
	   		    !cause_intr_tbl[level].handler) {
		return -1;
	} else {
		cause_intr_tbl[level].handler = (intr_func_t)NULL;
		cause_intr_tbl[level].intr_spl_mask = 0;
	}
	return 0;
}


/* XXX - Shouldn't need this in SN0. */
/*
 * Handle very small window in spl routines where an interrupt can occur,
 * but shouldn't:  an mtc0 to the SR requires a "few" cycles (<=4) to take
 * effect, and an event which arrives within that window will generate an
 * interrupt based upon the old SR, not the new SR.  However, the exception
 * handler reads the new SR, which may show   (SR & CAUSE) == 0   and make
 * intr() wonder why an interrupt was generated.  Just ignore the interrupt
 * and presume that we'll handle it when the SR really wants to enable it.
 */
int bad_intr_count = 0;
static void
nointr(void)
{
    bad_intr_count++;
}

#define LOCAL_HUB_UPDATE_MASK0(new_mask)		\
    (private.p_slice ?					\
       (LOCAL_HUB_S(PI_INT_MASK0_B, (new_mask)),	\
        LOCAL_HUB_L(PI_INT_MASK0_B)) :			\
       (LOCAL_HUB_S(PI_INT_MASK0_A, (new_mask)),	\
        LOCAL_HUB_L(PI_INT_MASK0_A)))

/*
 * intpend0
 *
 *   Dispatch interrupt that has come in on INTPEND0's level.
 */

/* ARGSUSED */
static void
intpend0(eframe_t *ep)
{
    hubreg_t pend;
    hubreg_t *masks = private.p_intmasks.intpend0_masks;
    int	level;


    /*
     * Handle all interrupts at this spl level.  Interrupts at higher
     * levels will be taken immediately.
     */

    while ((pend = LOCAL_HUB_L(PI_INT_PEND0) & masks[0]) != 0)
	do {
	    SYSINFO.intr_svcd++;

	    level = ms1bit(pend);

	    LOCAL_HUB_CLR_INTR(level);

	    /* XXX - Move this flag. */
	    if (hub_intrvect0[level].iv_tflags & THD_OK) {
		INTR_CALL_PROLOGUE_HANDLER(hub_intrvect0, level, ep);
#ifdef ITHREAD_LATENCY
		xthread_set_istamp(hub_intrvect0[level].iv_lat);
#endif
		/*
		 * default value of hub_intr_wakeup_cnt is 1.
		 * Call cvsema instead of vsema when you
		 * know that the ithread is already running.
		 * There is a race if you calling only cvsema
	 	 * all the time.
		 */
		if (valusema(&hub_intrvect0[level].iv_isync) >= hub_intr_wakeup_cnt) {
			cvsema(&hub_intrvect0[level].iv_isync);
		} else {
			/* Queue up the thread. */
			vsema(&hub_intrvect0[level].iv_isync);
		}
	    } else
		/* Call the handler the old-fashioned way. */
		INTR_CALL_HANDLER(hub_intrvect0, level, ep);

	    pend ^= 1ULL << level;
	} while (pend);

   /*
    * adjust accounting, since one extra was added in "intr".
    */

    SYSINFO.intr_svcd--;
}

/*
 * intpend1
 *
 *   Dispatch interrupt that has come in on INTPEND1's level.
 */


/* ARGSUSED */
static void
intpend1(eframe_t *ep)
{
    hubreg_t pend;
    hubreg_t *masks = private.p_intmasks.intpend1_masks;
    int level;

    ASSERT(N_INTPEND1_MASKS == 1);

    /*
     * Handle all interrupts at this spl level.  Interrupts at higher
     * levels will be taken immediately.
     */

    pend = LOCAL_HUB_L(PI_INT_PEND1) & masks[0];

    while (pend) {
	SYSINFO.intr_svcd++;

	level = ms1bit(pend);

	LOCAL_HUB_CLR_INTR(level + N_INTPEND_BITS);

	if (hub_intrvect1[level].iv_func == 0) {
		cmn_err(CE_PANIC, "attempt to invoke address 0 for level %d\n",
				level);
	} else
		INTR_CALL_HANDLER(hub_intrvect1, level, ep);

	pend ^= 1ULL << level;
    }

    /*
     * adjust accounting, since one extra was added in "intr".
     */

    SYSINFO.intr_svcd--;
}

/*
 * intr()
 *	Called from VEC_int, this routine loops through all pending
 *	interrupts of sufficiently high level, and dispatches to the
 *	appropriate interrupt handler.
 *
 *	On entrance, interrupts are disabled.
 */

/* ARGSUSED */
int
intr(
    eframe_t *ep,
    uint code,		/* XXX - Don't need this. */
    uint sr,
    register uint cause)
{
    int check_kp = 0;
    int check_exit = 0;

    CHECK_DELAY_TLBFLUSH_INTR(ENTRANCE, check_exit);

#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
    spldebug_log_event(ep->ef_epc);
    spldebug_log_event(cause);
#endif
    LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTRENTRY, NULL, NULL,
		    NULL, NULL);

#ifdef SABLE
    {
	volatile hubreg_t intpend0 = LOCAL_HUB_L(PI_INT_PEND0);
	volatile hubreg_t intpend1 = LOCAL_HUB_L(PI_INT_PEND1);
	volatile hubreg_t intmask0a = LOCAL_HUB_L(PI_INT_MASK0_A);
	volatile hubreg_t intpend0b = LOCAL_HUB_L(PI_INT_MASK0_B);

	static int first = 1;

	if (first) {
		cmn_err(CE_NOTE, "intr: cause 0x%x level 0x%x",
			cause, 0xbad);
	    first = 0;
	}
    }

#endif /* SABLE */

    ASSERT((getsr() & SR_IMASK) == 0); /* everything now blocked */

    if ((cause & sr & SR_IMASK) == 0) {
 	/* should not be able to get here - if we do we have a potential
	 * kernel preemption problem.
	 */
#ifdef DEBUG
    	cmn_err(CE_WARN|CE_CPUID,"intr: cause 0x%x sr 0x%x", cause, sr);
#endif

	nointr();
	goto exit_intr;
    }

    /*
     * Priority of CAUSE register interrupts top priority first.
     *
     *	SRB_ERR		IP7 - Hub errors
     *	SRB_DEV1	IP4 - Device and hub errors
     *	SRB_PROFCLK	IP6 - Profiling clock
     *	SRB_SCHEDCLK	IP8 - Scheduling clock
     *
     *	SRB_TIMOCLK	IP5 - Realtime clock (?)
     *
     *	SRB_DEV0	IP3 - Devices
     *	SRB_NET		IP2 - Soft net
     *	SRB_SWTIMO	IP1 - Soft timeout
     */

    /*
     * Comments about argument passing.
     * Interrupt handlers are passed ONE argument.
     * First level interrupt handlers (i.e. system interrupt handlers )
     * would get the eframe pointer as the first argument.
     * Second level interrupt handlers (i.e. the ones that are 
     * registered by the device drivers and other parts of I/O subsystem)
     * would get the argument they asked to be passed as the only
     * argument. Perhaps we can pass the eframe as the second argument
     * if needed (not done now)
     *
     * So, functions called via DO_INTR which happen to be the first
     * level interrupt handlers get 'ep' as the pointer.
     */

#define DO_INTR(_level)							\
    {									\
	ASSERT(cause_intr_tbl[_level].intr_spl_mask);			\
	check_kp = cause_intr_tbl[_level].check_kp;			\
	splx(cause_intr_tbl[_level].intr_spl_mask);			\
	ASSERT(cause_intr_tbl[_level].handler);				\
	cause_intr_tbl[_level].handler((void *)ep);			\
    }

    /*
     * This isn't a great way to do this, but a table lookup would have
     * too much space and complexity overhead for infinitesimal benefit.
     */

    SYSINFO.intr_svcd++;

    if (cause & SRB_ERR) {
	DO_INTR(SRB_ERR_IDX);		/* Hub chip errors */
    } else if (cause & SRB_DEV1) {
	DO_INTR(SRB_DEV1_IDX);		/* INT_PEND1 interrupt */
    } else if (cause & SRB_PROFCLK) {
	DO_INTR(SRB_PROFCLK_IDX);	/* Profiling interrupt */
    } else if (cause & SRB_SCHEDCLK) {
	DO_INTR(SRB_SCHEDCLK_IDX);	/* Scheduling clock */
    } else if (cause & SRB_TIMOCLK) {
	DO_INTR(SRB_TIMOCLK_IDX);	/* Timeouts */
    } else if (cause & SRB_SWTIMO) {
	DO_INTR(SRB_SWTIMO_IDX);	/* Timeouts */
    } else if (cause & SRB_DEV0) {
	DO_INTR(SRB_DEV0_IDX);		/* INT_PEND0 interrupt */
    } else if (cause & SRB_NET) {
	DO_INTR(SRB_NET_IDX);		/* Software networking interrupt */
    } else
	intr_stray(ep); 

exit_intr:
    CHECK_DELAY_TLBFLUSH_INTR(EXIT, check_exit);
    LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_INTRENTRY, NULL, NULL, NULL);
    return check_kp;
}


/*
 * setrtvector - set the interrupt handler for the scheduling clock.
 */
void
setrtvector(intr_func_t func)
{
    int rv;
    /* setvector(INTR_COUNTER_INTR, func); */
    /* XXX - more fake code. */
    /* sched_intr = func; */
    rv = cause_intr_connect(SRB_SCHEDCLK_IDX, func, SR_HI_MASK|SR_IE);
    rv = rv;
    ASSERT(rv != -1);
}

/*
 * setkgvector - set the interrupt handler for the profiling clock.
 */
void
setkgvector(void (*func)())
{
    int rv;
    rv = cause_intr_connect(SRB_PROFCLK_IDX, func, SR_PROF_MASK|SR_IE);
    rv = rv;
    ASSERT(rv != -1);
}

/*
 * Should never receive an exception while running on the idle 
 * stack.  It IS possible to handle *interrupts* while on the
 * idle stack, but a non-interrupt *exception* is a problem.
 */
void
idle_err(inst_t *epc, uint cause, void *fep, void *sp)
{
	eframe_t *ep = (eframe_t *)fep;

    if ((cause & CAUSE_EXCMASK) == EXC_IBE ||
	(cause & CAUSE_EXCMASK) == EXC_DBE) {
	(void)dobuserre((eframe_t *)ep, epc, 0);
    }

    /* XXX - This will have to change to deal with various SN0 errors. */
    cmn_err(CE_PANIC,
	    "exception on IDLE stack "
	    "ep:0x%x epc:0x%x cause:0x%w32x sp:0x%x badvaddr:0x%x",
	    ep, epc, cause, sp, getbadvaddr());
    /* NOTREACHED */
}


/*
 * earlynofault - handle very early global faults - usually just while
 *      sizing memory
 * Returns: 1 if should do nofault
 *          0 if not
 */
/* ARGSUSED */
int
earlynofault(eframe_t *ep, uint code)
{
	switch(code) {
	case EXC_DBE:
		return(1);
	default:
		return(0);
	}
}


/*
 * sendintr()
 * 	Send an interrupt to the specified level on the specified hub.
 * 	The SN0 hub is incapable of directing an interrupt to a particular
 * 	CPU so the sender must know that the interrupt is masked appropriately.
 */
/*ARGSUSED*/
int
sendintr(cpuid_t destid, unchar status)
{
	int level;

        ASSERT((status == DOACTION) || (status == DOTLBACTION));
	ASSERT((destid >= 0) &&  (destid < MAX_NUMBER_OF_CPUS()));
#if (CPUS_PER_NODE == 2)

	/*
	 * CPU slice A gets level CPU_ACTION_A
	 * CPU slice B gets level CPU_ACTION_B
	 */
	if (status == DOACTION)
		level = CPU_ACTION_A + cputoslice(destid);
	else	/* DOTLBACTION */
		level = N_INTPEND_BITS + TLB_INTR_A + cputoslice(destid);

	/*
	 * Convert the compact hub number to the NASID to get the correct
	 * part of the address space.  Then set the interrupt bit associated
 	 * with the CPU we want to send the interrupt to.
	 */
	REMOTE_HUB_SEND_INTR(COMPACT_TO_NASID_NODEID(cputocnode(destid)), level);

        return 0;

#else
	<< Bomb!  Must redefine this for more than 2 CPUS. >>
#endif
}

/* ARGSUSED */
static void
cpuintr(void *arg1, void *arg2)
{
#if RTE
	static int rte_intrdebug = 1;
#endif
	/*
	 * Frame Scheduler
	 */
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_CPUINTR, NULL, NULL,
			 NULL, NULL);

	/*
	 * Hardware clears the IO interrupts, but we need to clear software-
	 * generated interrupts.
	 */
	LOCAL_HUB_CLR_INTR(CPU_ACTION_A + cputoslice(cpuid()));

#if 0
	/* XXX - Handle error interrupts. */
	if (error_intr_reason)
		error_intr();
#endif /* 0 */

	/*
	 * If we're headed for panicspin and it is due to a NMI, save the
	 * eframe in the NMI area
	 */
	if (private.p_va_panicspin && nmied) {
		caddr_t	nmi_save_area;

		nmi_save_area = (caddr_t) (TO_UNCAC(TO_NODE(
			cputonasid(cpuid()), IP27_NMI_EFRAME_OFFSET)) + 
			cputoslice(cpuid()) * IP27_NMI_EFRAME_SIZE);
		bcopy((caddr_t) arg2, nmi_save_area, sizeof(eframe_t));
	}

	doacvec();
#if RTE
	if (private.p_flags & PDAF_ISOLATED && !rte_intrdebug)
		goto end_cpuintr;
#endif
	doactions();
#if RTE
end_cpuintr:
#endif
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_CPUINTR, NULL, NULL, NULL);
}


void
install_cpuintr(cpuid_t cpu)
{
	int		intr_bit = CPU_ACTION_A + cputoslice(cpu);

	if (intr_connect_level(cpu, intr_bit, INTPEND0_MAXMASK,
				(intr_func_t) cpuintr, NULL, NULL))
		cmn_err(CE_PANIC, "install_cpuintr: Can't connect interrupt.");
}

#ifdef DEBUG_INTR_TSTAMP
/* We allocate an array, but only use element number 64.  This guarantees that
 * the entry is in a cacheline by itself.
 */
#define DINTR_CNTIDX	32
#define DINTR_TSTAMP1	48
#define	DINTR_TSTAMP2	64
volatile long long dintr_tstamp_cnt[128];
int dintr_debug_output=0;
extern void idbg_tstamp_debug(void);
#ifdef SPLDEBUG
extern void idbg_splx_log(int);
#endif
#if DEBUG_INTR_TSTAMP_DEBUG
int dintr_enter_symmon=1000;	/* 1000 microseconds is 1 millisecond */
#endif

/* ARGSUSED */
static void
cpulatintr(void *arg)
{
	/*
	 * Hardware only clears IO interrupts so we have to clear our level
	 * here.
	 */
	LOCAL_HUB_CLR_INTR(CPU_INTRLAT_A + cputoslice(cpuid()));

#if DEBUG_INTR_TSTAMP_DEBUG
	dintr_tstamp_cnt[DINTR_TSTAMP2] =  GET_LOCAL_RTC;
	if ((dintr_tstamp_cnt[DINTR_TSTAMP2] - dintr_tstamp_cnt[DINTR_TSTAMP1])
	    > dintr_enter_symmon) {
#ifdef SPLDEBUG
		extern int spldebug_log_off;

		spldebug_log_off = 1;
#endif /* SPLDEBUG */
		debug("ring");
#ifdef SPLDEBUG
		spldebug_log_off = 0;
#endif /* SPLDEBUG */
	}
#endif
	dintr_tstamp_cnt[DINTR_CNTIDX]++;

	return;
}

static int install_cpulat_first=0;

void
install_cpulatintr(cpuid_t cpu)
{
	int		intr_bit;
	vertex_hdl_t	cpuv = cpuid_to_vertex(cpu);

	intr_bit = CPU_INTRLAT_A + cputoslice(cpu);
	if (intr_bit != intr_reserve_level(cpu, intr_bit, II_THREADED,
					   cpuv, "intrlat"))
		cmn_err(CE_PANIC,
			"install_cpulatintr: Can't reserve interrupt.");

	if (intr_connect_level(cpu, intr_bit, INTPEND0_MAXMASK,
				cpulatintr, NULL, NULL))
		cmn_err(CE_PANIC,
			"install_cpulatintr: Can't connect interrupt.");

	if (!install_cpulat_first) {
		install_cpulat_first++;
		idbg_addfunc("tstamp_debug", (void (*)())idbg_tstamp_debug);
#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
		idbg_addfunc("splx_log", (void (*)())idbg_splx_log);
#endif /* SPLDEBUG || SPLDEBUG_CPU_EVENTS */
	}
}

#endif /* DEBUG_INTR_TSTAMP */

/* ARGSUSED */
static void
dbgintr(void *arg)
{
	/*
	 * Hardware only clears IO interrupts so we have to clear our level
	 * here.
	 */
	LOCAL_HUB_CLR_INTR(N_INTPEND_BITS + DEBUG_INTR_A + cputoslice(cpuid()));

	debug("zing");
	return;
}


void
install_dbgintr(cpuid_t cpu)
{
	int		intr_bit;
	vertex_hdl_t	cpuv = cpuid_to_vertex(cpu);

	intr_bit = N_INTPEND_BITS + DEBUG_INTR_A + cputoslice(cpu);
	if (intr_bit != intr_reserve_level(cpu, intr_bit, 1, cpuv, "DEBUG"))
		cmn_err(CE_PANIC, "install_dbgintr: Can't reserve interrupt. "
			" intr_bit %d" ,intr_bit);

	if (intr_connect_level(cpu, intr_bit, INTPEND1_MAXMASK,
				dbgintr, NULL, NULL))
		cmn_err(CE_PANIC, "install_dbgintr: Can't connect interrupt.");

#ifdef DEBUG_INTR_TSTAMP
	/* Set up my interrupt latency test interrupt */
	install_cpulatintr(cpu);
#endif
}

/* ARGSUSED */
static void
tlbintr(void *arg)
{
	extern void tlbflush_rand(void);

	/*
	 * Hardware only clears IO interrupts so we have to clear our level
	 * here.
	 */
	LOCAL_HUB_CLR_INTR(N_INTPEND_BITS + TLB_INTR_A + cputoslice(cpuid()));

	tlbflush_rand();
	return;
}


void
install_tlbintr(cpuid_t cpu)
{
	int		intr_bit;
	vertex_hdl_t	cpuv = cpuid_to_vertex(cpu);

	intr_bit = N_INTPEND_BITS + TLB_INTR_A + cputoslice(cpu);
	if (intr_bit != intr_reserve_level(cpu, intr_bit, 1, cpuv, "DEBUG"))
		cmn_err(CE_PANIC, "install_tlbintr: Can't reserve interrupt. "
			" intr_bit %d" ,intr_bit);

	if (intr_connect_level(cpu, intr_bit, INTPEND1_MAXMASK,
				tlbintr, NULL, NULL))
		cmn_err(CE_PANIC, "install_tlbintr: Can't connect interrupt.");

}


/*
 * Send an interrupt to all nodes.  Don't panic if we get an error.
 * Returns 1 if any exceptions occurred.
 */
int
protected_broadcast(hubreg_t intrbit)
{
	nodepda_t *npdap = private.p_nodepda;
	int byte, bit;
	int error = 0;

	extern int _wbadaddr_val(volatile void *, int, volatile int *);

	/* Send rather than clear an interrupt. */
	intrbit |= 0x100;
	
	for (byte = 0; byte < NASID_MASK_BYTES; byte++) {
		for (bit = 0; bit < 8; bit++) {
			if (npdap->nasid_mask[byte] & (1 << bit)) {
				nasid_t nasid = byte * 8 + bit;
				error += _wbadaddr_val(REMOTE_HUB_ADDR(nasid,
							      PI_INT_PEND_MOD),
					      sizeof(hubreg_t),
					      (volatile int *)&intrbit);
			}
		}
	}

	return error;
}


/*
 * Poll the interrupt register to see if another cpu has asked us
 * to drop into the debugger (without lowering spl).
 */
void
chkdebug(void)
{
	if (LOCAL_HUB_L(PI_INT_PEND1) & (1L << (DEBUG_INTR_A + cputoslice(cpuid()))))
		dbgintr((void *)NULL);
}


/*
 * Install special graphics interrupt.
 */
void
install_gfxintr(cpuid_t cpu, ilvl_t swlevel, intr_func_t intr_func, void *intr_arg)
{
	int intr_bit = GFX_INTR_A + cputoslice(cpu);

	if (intr_connect_level(cpu, intr_bit, swlevel,
				intr_func, intr_arg, NULL))
		cmn_err(CE_PANIC, "install_cpuintr: Can't connect interrupt.");
}


/*
 * Install page migration interrupt handler.
 */
void
hub_migrintr_init(cnodeid_t cnode)
{
	cpuid_t cpu = cnodetocpu(cnode);
	int intr_bit = INT_PEND0_BASELVL + PG_MIG_INTR;

	if (numnodes == 1){
		/* 
		 * No migration with just one node..
		 */
		return;
	}
	
	if (cpu != -1) {
		if (intr_connect_level(cpu, intr_bit, 0,
			       (intr_func_t) migr_intr_handler, 0, (intr_func_t) migr_intr_prologue_handler))
			cmn_err(CE_PANIC, 
				"hub_migrintr_init: Can't connect interrupt.");
	}
}


/*
 * Cause all CPUs to stop by sending them each a DEBUG interrupt.
 * Parameter is actually a (cpumask_t *).
 */
void
debug_stop_all_cpus(void *stoplist)
{
	int cpu;
	ulong level;

	for (cpu=0; cpu<maxcpus; cpu++) {
		if (cpu == cpuid())
			continue;
		if (!cpu_enabled(cpu))
		        continue;
		/* "-1" is the old style parameter OR could be the new style
		 * if no-one is currently stopped.  We only stop the
		 * requested cpus, the others are already stopped (probably
		 * at a breakpoint).
		 */

		if (((cpumask_t *)stoplist != (cpumask_t *)-1LL) &&
		    (!CPUMASK_TSTB(*(cpumask_t*)stoplist, cpu)))
			continue;

#if (CPUS_PER_NODE == 2)

		/*
		 * CPU slice A gets level DEBUG_INTR_A
		 * CPU slice B gets level DEBUG_INTR_B
		 */
		level = DEBUG_INTR_A + get_cpu_slice(cpu);
		/*
		 * Convert the compact hub number to the NASID to get the
		 * correct part of the address space.  Then set the interrupt
		 * bit associated with the CPU we want to send the interrupt
		 * to.
		 */
		REMOTE_HUB_SEND_INTR(COMPACT_TO_NASID_NODEID(get_cpu_cnode(cpu)),
			N_INTPEND_BITS +  level);

#else
	<< Bomb!  Must redefine this for more than 2 CPUS. >>
#endif

	}
#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
        {
	    int cnode;
	    extern nasid_t partition_nasidtable[];

	    for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode++) {
		nasid_t node = partition_nasidtable[cnode];
		if (node == INVALID_NASID) break;
		if (NASID_TO_COMPACT_NODEID(node) == INVALID_CNODEID) {
		    REMOTE_HUB_SEND_INTR(node,
					 N_INTPEND_BITS + DEBUG_INTR_A);
		    REMOTE_HUB_SEND_INTR(node,
					 N_INTPEND_BITS + DEBUG_INTR_B);
		}
	    }
	}
#endif /* CELL_IRIX && LOGICAL_CELLS */
}


struct hardwired_intr_s {
	signed char level;
	int flags;
	char *name;
} const hardwired_intr[] = {
	{ INT_PEND0_BASELVL + RESERVED_INTR,	0,	"Reserved" },
	{ INT_PEND0_BASELVL + GFX_INTR_A,	0, 	"Gfx A" },
	{ INT_PEND0_BASELVL + GFX_INTR_B,	0, 	"Gfx B" },
	{ INT_PEND0_BASELVL + PG_MIG_INTR,	II_THREADED, "Migration" },
	{ INT_PEND0_BASELVL + UART_INTR,	0,	"Hub I2C" },
	{ INT_PEND0_BASELVL + CC_PEND_A,	0,	"Crosscall A" },
	{ INT_PEND0_BASELVL + CC_PEND_B,	0,	"Crosscall B" },
	{ INT_PEND0_BASELVL + MSC_MESG_INTR,	II_THREADED, "MSC Message" },
	{ INT_PEND0_BASELVL + CPU_ACTION_A,	0,	"CPU Action A" },
	{ INT_PEND0_BASELVL + CPU_ACTION_B,	0,	"CPU Action B" },
	{ INT_PEND1_BASELVL + IO_ERROR_INTR,	II_ERRORINT, "IO Error" },
	{ INT_PEND1_BASELVL + CLK_ERR_INTR,	II_ERRORINT, "Clock Error" },
	{ INT_PEND1_BASELVL + COR_ERR_INTR_A,	II_ERRORINT, "Correctable Error A" },
	{ INT_PEND1_BASELVL + COR_ERR_INTR_B,	II_ERRORINT, "Correctable Error B" },
	{ INT_PEND1_BASELVL + MD_COR_ERR_INTR,	II_ERRORINT, "MD Correct. Error" },
	{ INT_PEND1_BASELVL + NI_ERROR_INTR,	II_ERRORINT, "NI Error" },
	{ INT_PEND1_BASELVL + NI_BRDCAST_ERR_A,	II_ERRORINT, "Remote NI Error"},
	{ INT_PEND1_BASELVL + NI_BRDCAST_ERR_B,	II_ERRORINT, "Remote NI Error"},
	{ INT_PEND1_BASELVL + MSC_PANIC_INTR,	II_ERRORINT, "MSC Panic" },
	{ INT_PEND1_BASELVL + LLP_PFAIL_INTR_A,	II_ERRORINT, "LLP Pfail WAR" },
	{ INT_PEND1_BASELVL + LLP_PFAIL_INTR_B,	II_ERRORINT, "LLP Pfail WAR" },
	{ -1, 0, (char *)NULL}
};

/*
 * Reserve all of the hardwired interrupt levels so they're not used as
 * general purpose bits later.
 */
void
intr_reserve_hardwired(cnodeid_t cnode)
{
	cpuid_t targ;
	int level;
	int i;

	targ = cnodetocpu(cnode);

	if (targ == CPU_NONE) {
#ifdef DEBUG
		cmn_err(CE_NOTE, "Node %d has no CPUs", cnode);
#endif
		return;
	}

	for (i = 0; hardwired_intr[i].level != -1; i++) {
		level = hardwired_intr[i].level;

		if (level != intr_reserve_level(targ, level,
						hardwired_intr[i].flags,
						(vertex_hdl_t) NULL,
						hardwired_intr[i].name))
			cmn_err(CE_PANIC,
				"intr_reserve_hardwired: "
				"Can't reserve level %d.",
				level);
	}
}

/*
 * Check and clear interrupts.
 */
/*ARGSUSED*/
static void
intr_clear_bits(nasid_t nasid, volatile hubreg_t *pend, int base_level,
		char *name)
{
	volatile hubreg_t bits;
	int i;

	/* Check pending interrupts */
	if ((bits = HUB_L(pend)) != 0) {
		for (i = 0; i < N_INTPEND_BITS; i++) {
			if (bits & (1 << i)) {
#ifdef INTRDEBUG
				cmn_err(CE_WARN,
					"Nasid %d interrupt bit %d set in %s",
					nasid, i, name);
#endif
				LOCAL_HUB_CLR_INTR(base_level + i);
			}
		}
	}
}

/*
 * Clear out our interrupt registers.
 */
void
intr_clear_all(nasid_t nasid)
{
	REMOTE_HUB_S(nasid, PI_INT_MASK0_A, 0);
	REMOTE_HUB_S(nasid, PI_INT_MASK0_B, 0);
	REMOTE_HUB_S(nasid, PI_INT_MASK1_A, 0);
	REMOTE_HUB_S(nasid, PI_INT_MASK1_B, 0);

	intr_clear_bits(nasid, REMOTE_HUB_ADDR(nasid, PI_INT_PEND0),
			INT_PEND0_BASELVL, "INT_PEND0");
	intr_clear_bits(nasid, REMOTE_HUB_ADDR(nasid, PI_INT_PEND1),
			INT_PEND1_BASELVL, "INT_PEND1");
}

/* 
 * Dump information about a particular interrupt vector.
 */
static void
dump_vector(intr_info_t *info, intr_vector_t *vector, int bit, hubreg_t ip,
		hubreg_t ima, hubreg_t imb, void (*pf)(char *, ...))
{
	hubreg_t value = 1LL << bit;

	pf("  Bit %02d: %s: func 0x%x arg 0x%x prefunc 0x%x\n",
		bit, info->ii_name,
		vector->iv_func, vector->iv_arg, vector->iv_prefunc);
	pf("   vertex 0x%x %s%s",
		info->ii_owner_dev,
		((info->ii_flags) & II_RESERVE) ? "R" : "U",
		((info->ii_flags) & II_INUSE) ? "C" : "-");
	pf("%s%s%s%s",
		ip & value ? "P" : "-",
		ima & value ? "A" : "-",
		imb & value ? "B" : "-",
		((info->ii_flags) & II_ERRORINT) ? "E" : "-");
	pf("%s", (vector->iv_tflags & THD_OK) ? "T" :
		 (((info->ii_flags) & II_THREADED) ? "t" : "-"));
	if (vector->iv_tflags & THD_OK)
		pf(" mustrun %d pri %d", vector->iv_mustruncpu, vector->iv_pri); 
	pf("\n");
}


/*
 * Dump information about interrupt vector assignment.
 */
void
intr_dumpvec(cnodeid_t cnode, void (*pf)(char *, ...))
{
	nodepda_t *npda;
	int ip, bit;
	intr_vecblk_t *dispatch;
	hubreg_t ipr, ima, imb;
	nasid_t nasid;

	nasid = COMPACT_TO_NASID_NODEID(cnode);

	if (nasid == INVALID_NASID) {
		pf("intr_dumpvec: Bad cnodeid: %d\n", cnode);
		return ;
	}
		

	npda = NODEPDA(cnode);

	for (ip = 0; ip < 2; ip++) {
		dispatch = ip ? &(npda->intr_dispatch1) : &(npda->intr_dispatch0);
		ipr = REMOTE_HUB_L(nasid, ip ? PI_INT_PEND1 : PI_INT_PEND0);
		ima = REMOTE_HUB_L(nasid, ip ? PI_INT_MASK1_A : PI_INT_MASK0_A);
		imb = REMOTE_HUB_L(nasid, ip ? PI_INT_MASK1_B : PI_INT_MASK0_B);

		pf("Node %d INT_PEND%d:\n", cnode, ip);

		if (dispatch->ithreads_enabled)
			pf(" Ithreads enabled\n");
		else
			pf(" Ithreads disabled\n");
		pf(" vector_count = %d, vector_state = %d\n",
			dispatch->vector_count,
			dispatch->vector_state);
		pf(" CPU A count %d, CPU B count %d\n",
 		   dispatch->cpu_count[0],
 		   dispatch->cpu_count[1]);
		pf(" &vector_lock = 0x%x\n",
			&(dispatch->vector_lock));
		for (bit = 0; bit < N_INTPEND_BITS; bit++) {
			if ((dispatch->info[bit].ii_flags & II_RESERVE) ||
			    (ipr & (1L << bit))) {
				dump_vector(&(dispatch->info[bit]),
					    &(dispatch->vectors[bit]),
					    bit, ipr, ima, imb, pf);
			}
		}
		pf("\n");
	}
}


/*
 * This routine is called once during startup.  Some (most!) interrupts
 * get registered before threads can be setup on their behalf.  We don't
 * worry about locking, since this is done while still single-threaded,
 * and if an interrupt comes in while we're traversing the table, it will
 * either run the old way, or, if it sees the THD_OK bit, then we've
 * already set up the thread, so it can just signal the thread's sema.
 */
void
enable_ithreads(void)
{
	cnodeid_t cnode;
	int bit;
	intr_vecblk_t *vecblkp;
	int s;
	extern int default_intr_pri;

	for (cnode = 0; cnode < maxnodes; cnode++) {
		intr_vector_t *ivp;

		vecblkp = &NODEPDA(cnode)->intr_dispatch0;
	
		ivp = vecblkp->vectors;	

		INTR_LOCK(vecblkp);

		vecblkp->ithreads_enabled = 1;

		for (bit = 0; bit < N_INTPEND_BITS; bit++) {
			if (ivp[bit].iv_tinfo.thd_flags & THD_REG) {
				ASSERT((ivp[bit].iv_tinfo.thd_flags & THD_ISTHREAD));
				ASSERT(!(ivp[bit].iv_tinfo.thd_flags & THD_OK));
				intr_thread_setup(&ivp[bit], cnode, bit,
							default_intr_pri);
			}
		}

		INTR_UNLOCK(vecblkp);
	}
}

/*
 * intpend0_intrd
 *
 * Wrapper for intpend0 interrupt threads.
 * Note: ipsema is not a normal p operation.  On each call, the ithread
 * is restarted.
 */
static void
intpend0_intrd(intr_vector_t *ivp)
{
#ifdef DEBUG
	if (ivp->iv_mustruncpu >= 0) {
		/* Called on each restart */
		ASSERT(cpuid() == ivp->iv_mustruncpu);
	}
#endif

#ifdef ITHREAD_LATENCY
	xthread_update_latstats(ivp->iv_lat);
#endif /* ITHREAD_LATENCY */
	ivp->iv_func(ivp->iv_arg);
	ipsema(&ivp->iv_isync);
	/* NOTREACHED */
}

/* Do the one-time thread initialization work to set up the containing
 * ithread.  Then, move into the tight interrupt handling loop.
 * By changing the ithread function, we make the thread restart directly
 * into the intpend0_intrd function after the ipsema() call.
 */
static void
intpend0_intrd_start(intr_vector_t *ivp)
{
	ASSERT(ivp->iv_mustruncpu >= 0);
	setmustrun(ivp->iv_mustruncpu);

	xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *)intpend0_intrd,
			(void *)ivp);
	atomicSetInt(&ivp->iv_tinfo.thd_flags, THD_INIT);
	ipsema(&ivp->iv_isync);  /* Comes out in intpend0_intrd */
	/* NOTREACHED */
}

static void
intr_thread_setup(intr_vector_t *ivp, cnodeid_t cnode, int bit, ilvl_t intr_swlevel) {
	char thread_name[32];

	sprintf(thread_name, "ip0intrd[%d,%d]", cnode, bit);

	/* XXX need to adjust priority whenever an interrupt is connected */
	xthread_setup(thread_name, intr_swlevel, &ivp->iv_tinfo,
			(xt_func_t *)intpend0_intrd_start,
			(void *)ivp);
}

#ifdef DEBUG_INTR_TSTAMP
/*
 * The following routine lets the device handler change from ithread
 * to interrupt stack operation.
 * NOTE: Assumes that driver knows what it's doing and that driver
 * has already performed a evintr_connect() call on a threaded level.
 */
void
thrd_intr_noithread(int level)
{
	atomicClearInt(&hub_intrvect0[level].iv_tflags, THD_OK);
}


/*
 * The following routine lets the device handler change from 
 * interrupt stack to ithread operation.
 * NOTE: Assumes that driver knows what it's doing and that driver
 * has already performed a thrd_evintr_connect call().
 */

void
thrd_intr_noistack(int level)
{
	atomicSetInt(&hub_intrvect0[level].iv_tflags, THD_OK);
}
#endif /* DEBUG_INTR_TSTAMP */

#ifdef DEBUG_INTR_TSTAMP
#ifdef ULI_TSTAMP
<<< BOMB - can not define DEBUG_INTR_TSTAMP and ULI_TSTAMP>>>
#endif
#if DEBUG_INTR_TSTAMP_DEBUG

void
idbg_tstamp_debug()
{
	qprintf("TSTAMP1 (sender) 0x%x  TSTAMP2 (recv) 0x%x  delta 0x%x\n",
		dintr_tstamp_cnt[DINTR_TSTAMP1],
		dintr_tstamp_cnt[DINTR_TSTAMP2],
		dintr_tstamp_cnt[DINTR_TSTAMP2]-dintr_tstamp_cnt[DINTR_TSTAMP1]);
#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
	{
	extern int idbg_splx_logtime(uint64_t);

	qprintf("Interrupt sent at %d  received at %d\n",
		idbg_splx_logtime(dintr_tstamp_cnt[DINTR_TSTAMP1]),
		idbg_splx_logtime(dintr_tstamp_cnt[DINTR_TSTAMP2]));
	}
#endif /* SPLDEBUG || SPLDEBUG_CPU_EVENTS */
}
#endif /* DEBUG_INTR_TSTAMP_DEBUG */

int
dintr_tstamp_noithread()
{
	extern void thrd_evintr_noithread(int);

	thrd_intr_noithread(CPU_INTRLAT_A);
	thrd_intr_noithread(CPU_INTRLAT_B);
	return 0;
}

int
dintr_tstamp_noistack()
{
	extern void thrd_evintr_noistack(int);

	thrd_intr_noistack(CPU_INTRLAT_A);
	thrd_intr_noistack(CPU_INTRLAT_B);
	return 0;
}

/* called to send intr_cnt interrupts to destcpu.
 * If destcpu == CPU_NONE, then routine picks one of the other cpus.
 * Returns time (in cycles) from sending interrupt until receiving cpu
 * responds.
 * If intr_cnt > 1, then mintime and maxtime return min and max response
 * times and TOTAL response time is returned to caller.
 */

long long
dintr_tstamp( cpuid_t destcpu, int intr_cnt, int *maxtime, int *mintime )
{
	int s, old_cnt;
	long long ts1, ts2;
	int totaltime, mint, maxt;

	if (intr_cnt == 0)
		return (long long)0;

	/* block all interrupts and don't let us migrate */
	s = spl7();
	if (destcpu == CPU_NONE) {
		destcpu = maxcpus - 1;
		if (destcpu == cpuid())
			destcpu--;
	}
	/* if user specified cpu, and it's us then return error */
	if (destcpu == cpuid()) {
		splx(s);
		return ((long long) -1);
	}
	if (intr_cnt == 1) {
		old_cnt = dintr_tstamp_cnt[DINTR_CNTIDX];
		ts1 = GET_LOCAL_RTC;
#if DEBUG_INTR_TSTAMP_DEBUG
		dintr_tstamp_cnt[DINTR_TSTAMP1] =  ts1;
#endif
	
#if (CPUS_PER_NODE == 2)
		/* trigger the interrupt */

		REMOTE_HUB_SEND_INTR(
			COMPACT_TO_NASID_NODEID(cputocnode(destcpu)),
			CPU_INTRLAT_A + cputoslice(destcpu));

#else
		<< Bomb!  Must redefine this for more than 2 CPUS. >>
#endif

#if DEBUG_INTR_TSTAMP_DEBUG
		/* we can drop spl since we will use receiver's timestamp.
		 * Avoids problem with deadlock-ing on tlbsync (a cpuaction)
		 * which can occur if we're sending to an ithread since the
		 * ithread will not run until the tlbsync completes IFF the
		 * tlbsync was initiated by the cpu we're sending to.
		 */
		splx(s);
#endif

		while (dintr_tstamp_cnt[DINTR_CNTIDX] == old_cnt)
#if defined (HUB_II_IFDR_WAR)
			/* Normally we just wait for interrupt to occur so we
			 * get accurate timestamps.  But this WAR induces
			 * a deadlock if the cpu we're sending to has already
			 * entered kick_one() at the request of another cpu
			 * which will wait until THIS cpu also enters
			 * kick_one() before proceeding.
			 */
			{
			  if (private.p_actionlist.todolist) {
			  	int s;
				
				s=splhi();
				doactions();
				splx(s);
			  }
			}
#endif
			;

#if DEBUG_INTR_TSTAMP_DEBUG
		ts2 = dintr_tstamp_cnt[DINTR_TSTAMP2];
#else
		ts2 = GET_LOCAL_RTC;

		splx(s);
#endif		
		if ((ts2 - ts1) < 0)
			cmn_err(CE_WARN, "BAD RTC 0x%x 0x%x", ts1, ts2);

		return(ts2 - ts1);
	}

	/* following code handles multiple interrupt events */
	maxt = totaltime = 0;
	mint = 1000000;
	old_cnt = dintr_tstamp_cnt[DINTR_CNTIDX];

	while (intr_cnt) {

		ts1 = GET_LOCAL_RTC;
#if DEBUG_INTR_TSTAMP_DEBUG
		dintr_tstamp_cnt[DINTR_TSTAMP1] =  ts1;
#endif

		/* trigger the interrupt */
		REMOTE_HUB_SEND_INTR(
			COMPACT_TO_NASID_NODEID(cputocnode(destcpu)),
			CPU_INTRLAT_A + cputoslice(destcpu));

		while (dintr_tstamp_cnt[DINTR_CNTIDX] == old_cnt)
#if defined (HUB_II_IFDR_WAR)
			/* Normally we just wait for interrupt to occur so we
			 * get accurate timestamps.  But this WAR induces
			 * a deadlock if the cpu we're sending to has already
			 * entered kick_one() at the request of another cpu
			 * which will wait until THIS cpu also enters
			 * kick_one() before proceeding.
			 */
			{
			  if (private.p_actionlist.todolist) {
			  	int s;

				s=splhi();
				doactions();
				splx(s);
			  }
			}
#endif
			;

		ts2 = GET_LOCAL_RTC;
		ts2 -= ts1;
		totaltime += ts2;
		if (ts2 > maxt)
			maxt = ts2;
		if (ts2 < mint)
			mint = ts2;
		intr_cnt--;
		old_cnt++;
	}
	splx(s);
	*maxtime = maxt;
	*mintime = mint;
	return totaltime;

}
#endif /* DEBUG_INTR_TSTAMP */
