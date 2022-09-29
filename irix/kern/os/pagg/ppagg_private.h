/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.4 $"

#ifndef _PAGG_PPAG_PRIVATE_H_
#define _PAGG_PPAG_PRIVATE_H_

/*
 * Return values from code that adds a page to an AS in a job.
 */
#define	ADD_NOT_FIRST	0
#define	ADD_WITH_LIMIT	1
#define	ADD_NOT_DONE	2
#define	ADD_RETRY	3

/*
 * Return values from code that deletes a page from an AS in a job.
 */
#define DEL_NOT_DONE	0
#define DEL_LAST	1
#define	DEL_WITH_LIMIT	2

/*
 * Process aggregate object.
 * This file describes the physical layer of the obkect. 
 * A process aggregate represents a collection of processes that need to
 * be grouped for a purpose like an array session or a batch job.
 * It contains various accounting structures that are used to keep track 
 * of the aggregate's resources.
 */
struct	vm_pool_s;

/*
 * Vm resources kept track for a pagg. This is needed for miser only. 
 * Array sessions don't update these info.
 */
typedef struct vm_resources_s {
	pgno_t	rss;		/* Resident set size of the process agg. */
	pgno_t  smem;		/* Total availsmem used by the pagg */
	pgno_t	rmem;		/* Total availrmem used by the pagg */
	pgno_t	rss_limit;	/* MAX rss for the pagg */
	struct vm_pool_s *vm_pool; /* Resource pool to which the pagg belongs */
	pfn_t	maxrss;		/* max pages in rss ever */
#ifdef MISER_ARRAY
	ushort_t	*pagecount;	/* count for each pfn in system */
#endif
} vm_resources_t;

typedef struct ppagg_s {
	bhv_desc_t	ppag_bhv;          /* Base behavior */
	vpag_type_t	ppag_type;	/* Type of process aggregate */

	lock_t		ppag_lock;	/* update lock */

	prid_t		ppag_prid;	/* project ID */
	time_t		ppag_start;	/* start time (secs since 1970) */
	time_t		ppag_ticks;	/* lbolt at start */
	pid_t		ppag_pid;	/* pid that started this session */
	ushort_t	ppag_flag;	/* various flags */
	char		ppag_nice;	/* initial nice value of as_pid */

	/* Accounting data */
	int		ppag_spilen;	/* length of Service Provider Info */
	char		*ppag_spi;	/* Service Provider Information */
	acct_timers_t	ppag_timers;	/* accounting timers */
	acct_counts_t	ppag_counts;	/* accounting counters */
	shacct_t	*ppag_shacct;	/* shadow accounting info */
	
	/*
	 * These two can be made general and apply to something more than VM
 	 * This is used right now to implement batch reservation scheme.
	 */
	vm_resources_t	ppag_vm_resource; /* Vm acct. info */

	sv_t		ppag_wait;	/* Vpag suspend signal variable */
} ppag_t;
	

/* ppag_flag values */

#define	PPAG_SUSPEND	0x0001
#define PPAG_NONEW	0x0002

#define	BHV_TO_PPAG(b)\
		(ASSERT(BHV_POSITION(b) == BHV_POSITION_BASE), \
                (ppag_t *)(BHV_PDATA(b)))
#define	BHV_TO_VPAG(b)\
		(ASSERT(BHV_POSITION(b) == BHV_POSITION_BASE), \
                (vpagg_t *)(BHV_VOBJ(b)))
		
void	ppag_init(void);
extern	int ppagg_create(vpagg_t *, pid_t, int, vpag_type_t);

#endif /* _PAGG_PPAG_PRIVATE_H_ */

