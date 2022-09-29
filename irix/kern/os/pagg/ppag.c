/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Revision: 1.18 $"

#include "sys/types.h"
#include "sys/debug.h"
#include "sys/page.h"
#include "sys/nodepda.h"
#include "ksys/behavior.h"
#include "sys/pda.h"
#include "sys/extacct.h"
#include "sys/kmem.h"
#include "sys/sema.h"
#include "sys/systm.h"
#include "sys/errno.h"
#include "sys/sat.h"
#include "ksys/vproc.h"
#include "ksys/vpag.h"
#include "ksys/vm_pool.h"
#include <sys/pfdat.h>
#include "ksys/vmmacros.h"
#include "os/as/as_private.h"
#include <ksys/rmap.h>
#include "ppagg_private.h"
#include "sys/cmn_err.h"
#include "sys/uthread.h"
#include <sys/space.h>
static struct zone *ppag_zone;

extern int 	do_sessacct;	/* array session accounting enabled flag */
extern int	miser_rss_account;/* should we do accurate miser counting */

	
static void ppag_write_acct_info(bhv_desc_t *, paggid_t, int, uthread_t *);

static void ppag_destroy(bhv_desc_t *, paggid_t);
static void ppag_setprid(bhv_desc_t *, prid_t);
static prid_t ppag_getprid(bhv_desc_t *);
static void ppag_setspinfo(bhv_desc_t *, char *, int);
static void ppag_getspinfo(bhv_desc_t *, char *, int);
static int  ppag_getspilen(bhv_desc_t *);
static void ppag_setspilen(bhv_desc_t *, int);
static int  ppag_spinfo_isdflt(bhv_desc_t *);
static void ppag_accum_stats(bhv_desc_t *, 
		struct acct_timers *, struct acct_counts *);
static void ppag_make_shadow_acct(bhv_desc_t *);
static void ppag_extract_arsess_info(bhv_desc_t *, arsess_t *);
static void ppag_extract_shacct_info(bhv_desc_t *, shacct_t *);
static void ppag_flush_acct(bhv_desc_t *, paggid_t, uthread_t *);
static int  ppag_restrict_ctl(bhv_desc_t *, vpag_restrict_ctl_t);
static int  ppag_update_vm_resource(bhv_desc_t *, int, pfd_t *, 
				__psunsigned_t, __psunsigned_t);
static void ppag_set_vm_resource_limits(bhv_desc_t *, pgno_t);
static void ppag_get_vm_resource_limits(bhv_desc_t *, pgno_t *);
static vpag_type_t ppag_get_type(bhv_desc_t *);
static int ppag_transfer_vm_pool(bhv_desc_t *, vm_pool_t *);
static int ppag_reservemem(bhv_desc_t *, pgno_t, pgno_t, pgno_t *);
static void ppag_unreservemem(bhv_desc_t *, pgno_t, pgno_t); 
static int ppag_suspend(bhv_desc_t *);
static void ppag_resume(bhv_desc_t *);



vpagg_ops_t ppag_ops = {
	BHV_IDENTITY_INIT_POSITION(BHV_POSITION_BASE),
	ppag_destroy,
	ppag_get_type,

	/* Array session operations */
	ppag_getprid,
	ppag_setprid,
	ppag_getspinfo,
	ppag_setspinfo,
	ppag_getspilen,
	ppag_setspilen,
	ppag_spinfo_isdflt,
	ppag_make_shadow_acct,
	ppag_accum_stats,
	ppag_extract_arsess_info,
	ppag_extract_shacct_info,
	ppag_flush_acct,
	ppag_restrict_ctl,

	/* Miser operations */
	ppag_update_vm_resource,
	ppag_set_vm_resource_limits,
	ppag_get_vm_resource_limits,
	ppag_transfer_vm_pool,
	ppag_reservemem,
	ppag_unreservemem,
	ppag_suspend,
	ppag_resume
};

/*
 * ppag_init:
 * overall boot-time initialization for process aggregates.
 */
void
ppag_init(void)
{
	eff_spilen = spilen;
	eff_sessaf = sessaf;
	bzero(dfltspi, MAX_SPI_LEN);
	ppag_zone = kmem_zone_init(sizeof(ppag_t), "Process aggregate");

}

/*
 * ppag_create:
 * ppag_create allocates and initializes a process aggregate.
 */
int
ppagg_create(vpagg_t *vpag, pid_t pid, int nice, vpag_type_t type)
{
	ppag_t *ppag;
	/* REFERENCED */
	int error;

	/* Dynamically allocate an ppag */
	ppag = kmem_zone_zalloc(ppag_zone, KM_SLEEP);

	ASSERT(ppag);

	init_spinlock(&ppag->ppag_lock, "ppagl", current_pid());

	/* Initialize other interesting fields. Note that caller is */
	/* responsible for initializing the paggid, prid and spi.   */
	ppag->ppag_pid = pid;
	ppag->ppag_start = time;
	ppag->ppag_ticks = lbolt;
	ppag->ppag_shacct = NULL;
	ppag->ppag_prid = dfltprid;

	/*
	 * The default VM resource limits are set at infinity.
 	 */
	ppag->ppag_vm_resource.rss_limit = RSS_INFINITY;
	ppag->ppag_vm_resource.vm_pool = GLOBAL_POOL;
#ifdef MISER_ARRAY
	if (type == VPAG_MISER) {
		ppag->ppag_vm_resource.pagecount = (ushort_t *)kmem_zalloc(
				(max_pageord * sizeof(pf_use_t)), KM_SLEEP);
	}
#endif
	ppag->ppag_type = type;

	ppag->ppag_nice = nice;

	bzero(&ppag->ppag_timers, sizeof(struct acct_timers));
	bzero(&ppag->ppag_counts, sizeof(struct acct_counts));

	ppag->ppag_spilen = eff_spilen;
	ppag->ppag_spi = NULL;

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&ppag->ppag_bhv, ppag, vpag, &ppag_ops);
	bhv_insert_initial(VPAG_BHV_HEAD(vpag), &ppag->ppag_bhv);

	return(0);
}

/*
 * ppag_destroy:
 * Destroy the process aggregate object. Called when the last reference
 * to the object goes away.
 */
/* ARGSUSED */
static void
ppag_destroy(bhv_desc_t *bhv, paggid_t paggid)
{
	ppag_t 	*ppag = BHV_TO_PPAG(bhv);
	vpagg_t *vpagg = BHV_TO_VPAG(&ppag->ppag_bhv);

	/* If paggid is NULL, vpag creation failed. Just release tables */
	if (paggid != INVALID_PAGGID) {
		/* If configured, do session accounting */
		if (do_sessacct)
			ppag_write_acct_info(&ppag->ppag_bhv, paggid, 0, curuthread);

		/* If is a miser ppag, call into miser to do any necessary
	 	 * cleanup */
		if (ppag->ppag_type == VPAG_MISER)
			miser_cleanup(vpagg);
		
	}

#ifdef MISER_ARRAY
	if (ppag->ppag_vm_resource.pagecount != NULL)
		kmem_free(ppag->ppag_vm_resource.pagecount, 
					(max_pageord * sizeof(pf_use_t)));
#endif
	ASSERT(ppag->ppag_vm_resource.smem == 0);
	ASSERT(ppag->ppag_vm_resource.rmem == 0);
	ASSERT(ppag->ppag_vm_resource.rss == 0);

	/* Release the resources used by this session */
	if (ppag->ppag_shacct != NULL) {
		(void) kern_free(ppag->ppag_shacct);
		ppag->ppag_shacct = NULL;
	}
	if (ppag->ppag_spi != NULL) {
		kmem_free(ppag->ppag_spi, ppag->ppag_spilen);
	}

	bhv_remove(&vpagg->vpag_bhvh, &ppag->ppag_bhv);

	/* arsess dynamically allocated. Release its resources. */
	spinlock_destroy(&ppag->ppag_lock);
	kmem_zone_free(ppag_zone, ppag);
}

/*
 * ppag_setprid:
 * set project id for the array session.
 */
static void
ppag_setprid(bhv_desc_t *bhv, prid_t newprid)
{
	ppag_t 	*ppag = BHV_TO_PPAG(bhv);
	int s;

	s = mp_mutex_spinlock(&ppag->ppag_lock);
	ppag->ppag_prid = newprid;
	mp_mutex_spinunlock(&ppag->ppag_lock, s);

}
	
/*
 * ppag_getprid:
 * get project id for the array session.
 */
static prid_t
ppag_getprid(bhv_desc_t *bhv)
{
	ppag_t 	*ppag = BHV_TO_PPAG(bhv);
	return ppag->ppag_prid;

}

/*
 * ppag_setspinfo:
 *	set the service provider info for the specified session to
 *	a specified value. If we haven't already allocated a buffer
 *	for it, do that too; on the other hand, if the new info is
 *	identical to the default info, deallocate the buffer if present.
 */
static void
ppag_setspinfo(bhv_desc_t *bhv, char *pspi, int len)
{
	char    *spibuf = NULL;
	ppag_t 	*ppag = BHV_TO_PPAG(bhv);
	int s;
	int copylen = (len < ppag->ppag_spilen) ? len : ppag->ppag_spilen;

	/* This is all trivial if the SPI length is 0 */
	if (ppag->ppag_spilen < 1) {
		return;
	}

	/* If the SPI ptr is null, take a chance and allocate storage */
	/* for a new buffer now before getting the lock. We'll free   */
	/* it later on if we don't need it after all.		      */
	if (ppag->ppag_spi == NULL  &&  pspi != NULL) {
		spibuf = kmem_alloc(ppag->ppag_spilen, KM_SLEEP);
		ASSERT(spibuf);
	}

	s = mp_mutex_spinlock(&ppag->ppag_lock);

	if (pspi == NULL) {
		if (ppag->ppag_spi != NULL) {
			spibuf = ppag->ppag_spi; /* this causes deallocation */
			ppag->ppag_spi = NULL;
		}
	}
	else {
		if (spibuf != NULL) {
			/* Make sure someone else hasn't allocated one since */
			/* the last time we looked			     */
			if (ppag->ppag_spi == NULL) {
				ppag->ppag_spi = spibuf;
				spibuf = NULL;
			}
		}

		bcopy(pspi, ppag->ppag_spi, copylen);
		if (copylen < ppag->ppag_spilen) {
			bzero(ppag->ppag_spi + copylen,
			      ppag->ppag_spilen - copylen);
		}
	}

	mp_mutex_spinunlock(&ppag->ppag_lock, s);

	if (spibuf != NULL) {
		kmem_free(spibuf, ppag->ppag_spilen);
	}
}

/*
 * ppag_setspilen:
 *	set the length of the service provider info for the specified
 *	session to a specified value, and truncate or extend the current
 *	data as needed.
 */
static void
ppag_setspilen(bhv_desc_t *bhv, int newlen)
{
	ppag_t 	*ppag = BHV_TO_PPAG(bhv);
	char    *newbuf = NULL;
	char	*oldbuf = ppag->ppag_spi;
	int	oldlen = ppag->ppag_spilen;
	int s;

	/* Ignore the case where nothing changes */
	if (oldlen == newlen) {
		return;
	}

	/* If the pagg has it's own SPI buffer, allocate a new one    */
	/* with the requested length and copy over the relevant data. */
	/* Don't use realloc because we haven't locked the pagg yet.  */
	if (oldbuf != NULL  &&  newlen > 0) {
		newbuf = kmem_alloc(newlen, KM_SLEEP);
		ASSERT(newbuf);

		if (newlen < oldlen) {
			bcopy(oldbuf, newbuf, newlen);
		}
		else {
			bcopy(oldbuf, newbuf, oldlen);
			bzero(newbuf + oldlen,  newlen - oldlen);
		}
	}

	/* Set up the new buffer and length under lock protection */
	s = mp_mutex_spinlock(&ppag->ppag_lock);

	ppag->ppag_spilen = newlen;
	ppag->ppag_spi = newbuf;

	mp_mutex_spinunlock(&ppag->ppag_lock, s);

	/* Release the old buffer if necessary */
	if (oldbuf != NULL) {
		kmem_free(oldbuf, oldlen);
	}
}

/*
 * ppag_getspinfo:
 *	get the service provider info for the specified array session
 */
static void
ppag_getspinfo(bhv_desc_t *bhv, char *pspi, int len)
{
	int     copylen;
	ppag_t 	*ppag = BHV_TO_PPAG(bhv);

	if (ppag->ppag_spi != NULL) {
		copylen = (len < ppag->ppag_spilen) ? len : ppag->ppag_spilen;
		bcopy(ppag->ppag_spi, pspi, copylen);
	}
	else {
		copylen = (len < eff_spilen) ? len : eff_spilen;
		bcopy(dfltspi, pspi, copylen);
	}

	if (copylen < len) {
		bzero(pspi + copylen, len - copylen);
	}
}
	
/*
 * ppag_getspilen:
 *	get the length of the service provider information for the pagg.
 */
static int
ppag_getspilen(bhv_desc_t *bhv)
{
	ppag_t 	*ppag = BHV_TO_PPAG(bhv);

	return ppag->ppag_spilen;
}

/*
 * ppag_spinfo_isdflt
 *	returns zero if the specified pagg has non-default service provider
 *	info, or non-zero if it is using the default service provider info.
 */
static int
ppag_spinfo_isdflt(bhv_desc_t *bhv)
{
	ppag_t *ppag = BHV_TO_PPAG(bhv);

	return (ppag->ppag_spi == NULL);
}

/*
 * ppag_accum_stats:
 *	accumulate statistics for a specified process in its array session
 */
static void
ppag_accum_stats(bhv_desc_t *bhv,
	     struct acct_timers *timers,
	     struct acct_counts *counts)
{
	ppag_t  *ppag = BHV_TO_PPAG(bhv);

	int s;

	s = mp_mutex_spinlock(&ppag->ppag_lock);

	ppag->ppag_timers.ac_utime  += timers->ac_utime;
	ppag->ppag_timers.ac_stime  += timers->ac_stime;
	ppag->ppag_timers.ac_bwtime += timers->ac_bwtime;
	ppag->ppag_timers.ac_rwtime += timers->ac_rwtime; 
	ppag->ppag_timers.ac_qwtime += timers->ac_qwtime;

	ppag->ppag_counts.ac_mem    += counts->ac_mem;
	ppag->ppag_counts.ac_swaps  += counts->ac_swaps;
	ppag->ppag_counts.ac_chr    += counts->ac_chr;
	ppag->ppag_counts.ac_chw    += counts->ac_chw;
	ppag->ppag_counts.ac_br     += counts->ac_br;
	ppag->ppag_counts.ac_bw     += counts->ac_bw;
	ppag->ppag_counts.ac_syscr  += counts->ac_syscr;
	ppag->ppag_counts.ac_syscw  += counts->ac_syscw;

	mp_mutex_spinunlock(&ppag->ppag_lock, s);
}

/*
 * ppag_make_shadowacct
 *      make a shadow copy of the current session accounting data. The
 *      shadow copy will be deducted from the final values that are reported
 *      when the array session terminates. This is typically done after
 *      restoring a checkpointed array session so that any time (etc.)
 *      that was presumably billed when the original array session
 *      terminated isn't billed a second time.
 */
static void
ppag_make_shadow_acct(bhv_desc_t *bhv)
{	
	ppag_t  *ppag = BHV_TO_PPAG(bhv);

	shacct_t *shacct = ppag->ppag_shacct;

	if (shacct == NULL) {
		shacct = kern_malloc(sizeof(shacct_t));
		shacct->sha_type = SHATYPE_SESS;
		ppag->ppag_shacct = shacct;
	}

	ASSERT(shacct->sha_type == SHATYPE_SESS);

	shacct->sha_ioch = 0;
	bcopy(&ppag->ppag_timers, &shacct->sha_timers, sizeof(acct_timers_t));
	bcopy(&ppag->ppag_counts, &shacct->sha_counts, sizeof(acct_counts_t));
}

/*
 * ppag_extract_arsess_info:
 * Extract the array session info for a given session. This is passed
 * to sat for updating sat records.
 */
static void
ppag_extract_arsess_info(bhv_desc_t *bhv, arsess_t *as)
{
	ppag_t  *ppag = BHV_TO_PPAG(bhv);

	as->as_prid = ppag->ppag_prid;
	as->as_start = ppag->ppag_start;
	as->as_ticks = ppag->ppag_ticks;
	as->as_pid = ppag->ppag_pid;
	as->as_spilen = ppag->ppag_spilen;
	as->as_flag = 0;
	as->as_nice = ppag->ppag_nice;

	if (ppag->ppag_shacct != NULL) {
		as->as_flag |= AS_GOT_CHGD_INFO;
	}
	if (ppag->ppag_flag & PPAG_NONEW) {
		as->as_flag |= AS_NEW_RESTRICTED;
	}

	if (ppag->ppag_spi != NULL) {
		bcopy(ppag->ppag_spi, as->as_spi, ppag->ppag_spilen);
	}
	else {
		bzero(as->as_spi, ppag->ppag_spilen);
	}

	as->as_timers = ppag->ppag_timers;
	as->as_counts = ppag->ppag_counts;
}

/*
 * ppag_extract_shacct_info:
 *	Extract the shadow accounting info for a given session
 */
static void
ppag_extract_shacct_info(bhv_desc_t *bhv, shacct_t *sh)
{
	ppag_t  *ppag = BHV_TO_PPAG(bhv);

	if (ppag->ppag_shacct) {
		bcopy(ppag->ppag_shacct, sh, sizeof(shacct_t));
	}
	else {
		bzero(sh, sizeof(shacct_t));
	}
}


/*
 * ppag_flush_acct:
 *	Flush the current array session accounting information out
 *	to an interim accounting record. Typically used in paranoia
 *	situations to ensure that a system outage or shutdown won't
 *	cause active array sessions to go unbilled. The accounting
 *	record will be written on behalf of the specified uthread.
 */
static void
ppag_flush_acct(bhv_desc_t *bhv, paggid_t paggid, uthread_t *ut)
{
	/* If array session accounting is turned on, go write a record */
	if (do_sessacct) {
		ppag_write_acct_info(bhv, paggid, AS_FLUSHED, ut);
	}

	/* Make a shadow copy of the current usage info */
	ppag_make_shadow_acct(bhv);
}

/*
 * ppagg_write_acct_info:
 *	write a session accounting record on behalf of the specified
 *	uthread. It is assumed that do_sessacct	has already been checked.
 */
static void
ppag_write_acct_info(bhv_desc_t *bhv, paggid_t paggid, int flgs, uthread_t *ut)
{
	arsess_t    arsess_info;
	ppag_t      *ppag = BHV_TO_PPAG(bhv);

	/* Copy relevant data to an arsess_t */
	ppag_extract_arsess_info(bhv, &arsess_info);
        arsess_info.as_handle = paggid;
	arsess_info.as_flag  |= flgs;

	/* Adjust the accounting data by any amount that has already */
	/* been reported (typically prior to a checkpoint)	     */
	if (ppag->ppag_shacct != NULL) {
		shacct_t *sh = ppag->ppag_shacct;

		ASSERT(sh->sha_type == SHATYPE_SESS);

		arsess_info.as_timers.ac_utime  -= sh->sha_timers.ac_utime;
		arsess_info.as_timers.ac_stime  -= sh->sha_timers.ac_stime;
		arsess_info.as_timers.ac_bwtime -= sh->sha_timers.ac_bwtime;
		arsess_info.as_timers.ac_rwtime -= sh->sha_timers.ac_rwtime;
		arsess_info.as_timers.ac_qwtime -= sh->sha_timers.ac_qwtime;

		arsess_info.as_counts.ac_mem    -= sh->sha_counts.ac_mem;
		arsess_info.as_counts.ac_swaps  -= sh->sha_counts.ac_swaps;
		arsess_info.as_counts.ac_chr    -= sh->sha_counts.ac_chr;
		arsess_info.as_counts.ac_chw    -= sh->sha_counts.ac_chw;
		arsess_info.as_counts.ac_br     -= sh->sha_counts.ac_br;
		arsess_info.as_counts.ac_bw     -= sh->sha_counts.ac_bw;
		arsess_info.as_counts.ac_syscr  -= sh->sha_counts.ac_syscr;
		arsess_info.as_counts.ac_syscw  -= sh->sha_counts.ac_syscw;
	}		

	/* Go cut the accounting record */
	_SAT_SESSION_ACCT(&arsess_info, ut, 0);
}

/*
 * ppag_restrict_ctl
 *	Manipulate and/or query restrictions made upon an array session
 */
static int
ppag_restrict_ctl(bhv_desc_t *bhv, vpag_restrict_ctl_t code)
{
	int  result = 0;
	ppag_t  *ppag = BHV_TO_PPAG(bhv);

	/* Proceed according to the request code */
	switch (code) {

	    case VPAG_ALLOW_NEW:
		ppag->ppag_flag &= ~PPAG_NONEW;
		break;

	    case VPAG_NEW_IS_RESTRICTED:
		result = ((ppag->ppag_flag & PPAG_NONEW) != 0);
		break;

	    case VPAG_RESTRICT_NEW:
		ppag->ppag_flag |= PPAG_NONEW;
		break;

	    default:
		/* unknown control code */
		result = -1;
	}

	return result;
}


/*
 * VM resource management routines.
 */

/*
 * ppag_update_vm_resource:
 * Check to see if the miser job has exceeded its smem/rmem limits. If so
 * return failure else update the cumulative smem/rmem for the job.
 */
#ifdef MISER_ARRAY

/*
 * This solution needs a per-job-per-page array, each of whose index
 * is of size pf_use_t. The basic philosophy is to maintain a pf_use_t
 * for each pfn being used by a job, and more efficient data structures
 * can be designed. This approach might be useful in the distributed
 * environment.
 */

int
ppag_update_vm_resource(bhv_desc_t *bhv, int op, pfd_t *pfd, 
			__psunsigned_t arg1, __psunsigned_t arg2)
{
	int 	s, ret = 0, docheck;
	int	npage = (int)arg1;
	ppag_t  *ppag = BHV_TO_PPAG(bhv);
	pgno_t	rslim, curss;
	ushort_t *countarray = ppag->ppag_vm_resource.pagecount;

#define DELETE_PAGE(pfd) (--countarray[pfn_to_ordinal_num(pfdattopfn(pfd))]==0)
#define INSERT_PAGE(pfd) (++countarray[pfn_to_ordinal_num(pfdattopfn(pfd))]==1)
	
	/*
	 * The foll asserts that miser does not work in the distributed
	 * case for now.
	 */
	if (pfd) {
		ASSERT((pfd >= PFD_LOW(0)) && (pfd <= PFD_HIGH(numnodes -1)));
	}
        s = mp_mutex_spinlock(&ppag->ppag_lock);
	rslim = ppag->ppag_vm_resource.rss_limit;
	curss = ppag->ppag_vm_resource.rss;
	docheck = (rslim != RSS_INFINITY);

	switch (op) {
		case JOBRSS_INC_FOR_PFD :
			if (INSERT_PAGE(pfd)) {
				if ((docheck) && ((curss + 1) > rslim)) {
					ret = ENOMEM;
					DELETE_PAGE(pfd);
				} else {
					ppag->ppag_vm_resource.rss += 1;
				}
			}
			break;
		case JOBRSS_INC_BLIND :
			if ((docheck) && ((curss + npage) > rslim)) {
				ret = ENOMEM;
			} else {
				ppag->ppag_vm_resource.rss += npage;
			}
			break;
		case JOBRSS_INS_PFD :
			INSERT_PAGE(pfd);
			break;
		case JOBRSS_INC_RELE_UNDO :
			if (INSERT_PAGE(pfd)) {
			} else {
				ppag->ppag_vm_resource.rss -= 1;
			}
			break;
		case JOBRSS_INC_RELE :
			if (DELETE_PAGE(pfd)) {
			} else {
				if ((docheck)&&((curss+1) > rslim)) {
					INSERT_PAGE(pfd);
					ret = ENOMEM;
				} else {
					ppag->ppag_vm_resource.rss += 1;
				}
			}
			break;
		case JOBRSS_FLIP_PFD_ADD :
			INSERT_PAGE(pfd);
			break;
		case JOBRSS_FLIP_PFD_DEL :
			DELETE_PAGE(pfd);
			break;
		case JOBRSS_DECREASE :
			if (DELETE_PAGE(pfd))
				ppag->ppag_vm_resource.rss -= 1;
			break;
		default :
			panic("wrong opcode to ppag_update_vm_resource");
			break;
	}

	ASSERT(ppag->ppag_vm_resource.rss >= 0);
	if (ppag->ppag_vm_resource.rss >= ppag->ppag_vm_resource.maxrss)
		ppag->ppag_vm_resource.maxrss = ppag->ppag_vm_resource.rss;

	mp_mutex_spinunlock(&ppag->ppag_lock, s);
	return(ret);
}

/*
 * Dummy routine so that we do not need to put miser_array stuff
 * into rmap.c.
 */
/* ARGSUSED */
miser_jobcount(void *obj, void *arg1, void *arg2)
{
	return(0);
}

#else

/*
 * This is invoked in the job scan cases, where we want to determine
 * if there are ASs in a job that already have pulled in the page.
 * When arg2 == 0, we are just looking for any other AS in the job,
 * when arg2 != 0, we want to count at least two other ASs in the
 * job using the page, and arg2 is used as a running count.
 */
int
miser_jobcount(void *obj, void *arg1, void *arg2)
{
	vpagg_t *ptevpag = (vpagg_t *)obj;
	vpagg_t *vpag = (vpagg_t *)arg1;

	if (arg2) {
		int	*counter = (int *)arg2;

		if (ptevpag == vpag)
			(*counter)++;
		return((*counter) == 2);
	} else {
		return(ptevpag == vpag);
	}
}

/*
 * If this is the last AS in the job using page, delete it and return 
 * DEL_LAST. Else, there are other ASs in job using the page, hence
 * return DEL_NOT_DONE if the page can not be replaced with a new one.
 * Else, delete the page and return DEL_WITH_LIMIT.
 */
static int
miser_rmap_del(vpagg_t *vpag, pfd_t *pfd, pde_t *pd, int limit)
{
	int twormore, ret, s;

	s = RMAP_LOCK(pfd);

	/*
	 * Is there two or more ASs in the job using the page?
	 */
	twormore = (miser_rss_account ? rmap_scanmap(pfd, RMAP_JOBRSS_TWO, 
						vpag) : 0);
	if (twormore) {
		if (limit) 
			ret = DEL_NOT_DONE;
		else
			ret = DEL_WITH_LIMIT;
	} else {
		ret = DEL_LAST;
	}
	if (ret != DEL_NOT_DONE) {
		/*
		 * Do the work of DELETE_MAPPING
		 */
#ifdef SN0
		if (pg_isfetchop(pd))
			fetchop_flush(pfd);
#endif
		rmap_delmap_nolock(pfd, pd);
	}

#ifdef DEBUG
	if (miser_rss_account && (ret == DEL_LAST)) {
 		int x = rmap_scanmap(pfd, RMAP_JOBRSS_ANY, vpag);
 		ASSERT(x == 0);
	}
	if (miser_rss_account && (ret == DEL_WITH_LIMIT)) {
 		int x = rmap_scanmap(pfd, RMAP_JOBRSS_ANY, vpag);
 		ASSERT(x == 1);
	}
#endif

	RMAP_UNLOCK(pfd, s);
	return(ret);
}

/*
 * If this is not the first AS in job using page, add to rmap, return 
 * ADD_NOT_FIRST; else, ie, if this is the first AS in job using page, 
 * add to rmap if limit permits, return ADD_WITH_LIMIT; else return 
 * ADD_NOT_DONE. Returns ADD_RETRY if all locks need to be dropped
 * and operation retried, because rmap routine could not get memory.
 */
static int
miser_rmap_add(vpagg_t *vpag, pfd_t *pfd, pde_t *pd, struct pm *pm, int limit)
{
	int notfirst, ret, s;

	s = RMAP_LOCK(pfd);

	/*
	 * Is there already another AS in the job using the page?
	 */
	notfirst = (miser_rss_account ? rmap_scanmap(pfd, RMAP_JOBRSS_ANY, 
					vpag) : 0);
	if (notfirst) {
		if (rmap_addmap_nolock(pfd, pd, pm))
			ret = ADD_RETRY;
		else
			ret = ADD_NOT_FIRST;
	} else if (limit) {
		ret = ADD_NOT_DONE;
	} else {
		if (rmap_addmap_nolock(pfd, pd, pm))
			ret = ADD_RETRY;
		else
			ret = ADD_WITH_LIMIT;
	}

#ifdef DEBUG
	if (miser_rss_account && (ret == ADD_NOT_FIRST)) {
		int x = rmap_scanmap(pfd, RMAP_JOBRSS_TWO, vpag);
		ASSERT(x == 1);
	}
	if (miser_rss_account && (ret == ADD_WITH_LIMIT)) {
		int x = rmap_scanmap(pfd, RMAP_JOBRSS_TWO, vpag);
		ASSERT(x == 0);
		x = rmap_scanmap(pfd, RMAP_JOBRSS_ANY, vpag);
		ASSERT(x == 1);
	}
#endif

	RMAP_UNLOCK(pfd, s);
	return(ret);
}

/*
 * ppag_update_vm_resource:
 * Check to see if the miser job has exceeded its smem/rmem limits. If so
 * return failure else update the cumulative smem/rmem for the job.
 */
ppag_update_vm_resource(bhv_desc_t *bhv, int op, pfd_t *pfd, 
			__psunsigned_t arg1, __psunsigned_t arg2)
{
	int 	s, ret = 0, docheck, retry, limit;
	int	npage = (int)arg1;
	ppag_t  *ppag = BHV_TO_PPAG(bhv);
	vpagg_t *vpag = BHV_TO_VPAG(bhv);
	pgno_t	rslim, curss;

	/*
	 * The foll asserts that miser does not work in the distributed
	 * case for now.
	 */
	if (pfd) {
		ASSERT((pfd >= PFD_LOW(0)) && (pfd <= PFD_HIGH(numnodes -1)));
	}
restart:
	retry = limit = 0;
        s = mp_mutex_spinlock(&ppag->ppag_lock);
	rslim = ppag->ppag_vm_resource.rss_limit;
	curss = ppag->ppag_vm_resource.rss;
	docheck = (rslim != RSS_INFINITY);

	switch (op) {
		case JOBRSS_INC_FOR_PFD :
			if ((docheck) && ((curss + 1) > rslim))
				limit = 1;
			switch (miser_rmap_add(vpag, pfd, (pde_t *)arg1, 
						(struct pm *)arg2, limit)) {
				case ADD_NOT_FIRST:
					break;
				case ADD_WITH_LIMIT:
					ppag->ppag_vm_resource.rss += 1;
					break;
				case ADD_NOT_DONE:
					ret = ENOMEM;
					break;
				case ADD_RETRY:
					retry = 1;
					break;
			}
			break;
		case JOBRSS_INC_BLIND :
			if ((docheck) && ((curss + npage) > rslim)) {
				ret = ENOMEM;
			} else {
				ppag->ppag_vm_resource.rss += npage;
			}
			break;
		case JOBRSS_INC_RELE_UNDO :
			switch (miser_rmap_add(vpag, pfd, (pde_t *)arg1,
					(struct pm *)arg2, 0)) {
				case ADD_NOT_FIRST:
					ppag->ppag_vm_resource.rss -= 1;
					break;
				case ADD_WITH_LIMIT:
				case ADD_NOT_DONE:
					break;
				case ADD_RETRY: 
					retry = 1;
					break;
			}
			break;
		case JOBRSS_INC_RELE :
			if ((docheck) && ((curss + 1) > rslim))
				limit = 1;
			switch(miser_rmap_del(vpag, pfd, (pde_t *)arg1, limit)) {
				case DEL_NOT_DONE:
					ret = ENOMEM;
					break;
				case DEL_WITH_LIMIT:
					ppag->ppag_vm_resource.rss += 1;
					break;
				case DEL_LAST:
					break;
			}
			break;
		case JOBRSS_DECREASE :
			if (miser_rmap_del(vpag, pfd, (pde_t *)arg1, 0) == 
								DEL_LAST) {
				ppag->ppag_vm_resource.rss -= 1;
			}
			break;
		case JOBRSS_FLIP_PFD_ADD :
		case JOBRSS_FLIP_PFD_DEL :
		case JOBRSS_INS_PFD :
			ASSERT(0);
			break;
		default :
			panic("wrong opcode to ppag_update_vm_resource");
			break;
	}

	ASSERT(ppag->ppag_vm_resource.rss >= 0);

	/*
	 * Record the max job rss till now.
	 */
	if (ppag->ppag_vm_resource.rss >= ppag->ppag_vm_resource.maxrss)
		ppag->ppag_vm_resource.maxrss = ppag->ppag_vm_resource.rss;

	mp_mutex_spinunlock(&ppag->ppag_lock, s);
	if (retry) {
		setsxbrk();
		goto restart;
	}
	return(ret);
}

#endif /* MISER_ARRAY */

/*
 * ppag_reservemem:
 * Reserve memory for a process in a given process aggregate. We do the
 * reservation here as  a ppag operation as we need to ensure the pool to 
 * which the pagg belongs does not change while we call reservemem. Here
 * we hold the ppag_lock and hence vm_pool does not change while we 
 * call reservemem.
 */
int
ppag_reservemem(bhv_desc_t *bhv,  pgno_t smem, pgno_t rmem, 
			pgno_t *pre_reserved_smem)
{
	ppag_t  *ppag = BHV_TO_PPAG(bhv);

	int s;
	int	error;
	vm_pool_t *vm_pool;

        s = mp_mutex_spinlock(&ppag->ppag_lock);

	ppag->ppag_vm_resource.rmem += rmem; 
	ppag->ppag_vm_resource.smem += smem; 

	vm_pool = ppag->ppag_vm_resource.vm_pool;

	/*
	 * Try to see if we can satisfy this with the 
	 * pre-reserved memory we acquired at exec time.
	 * We can do this if there is reserved memory &&
	 * we require only smem and not rmem and
	 * the job is still in GLOBAL_POOL
	 */

	if ((vm_pool == GLOBAL_POOL) 
		&& smem && !rmem && smem < *pre_reserved_smem) {
		*pre_reserved_smem -= smem;
		mp_mutex_spinunlock(&ppag->ppag_lock, s);
		return 0;
	}

	if (reservemem(vm_pool, smem, rmem, 0)) {


		ppag->ppag_vm_resource.rmem -= rmem;
		ppag->ppag_vm_resource.smem -= smem;

		if (vm_pool == GLOBAL_POOL) {
			vm_pool_set_wanted(vm_pool, smem, rmem);
			error =  EMEMRETRY;
		} else error = EAGAIN;

		mp_mutex_spinunlock(&ppag->ppag_lock, s);
		return error;
	}

	mp_mutex_spinunlock(&ppag->ppag_lock, s);
	return 0;
}

/*
 * ppag_unreservemem:
 * Give back reserved memory to the pool.
 */ 
void
ppag_unreservemem(bhv_desc_t *bhv,  pgno_t smem, pgno_t rmem) 
{
	ppag_t  *ppag = BHV_TO_PPAG(bhv);

	int s;
	vm_pool_t *vm_pool;

        s = mp_mutex_spinlock(&ppag->ppag_lock);

	ppag->ppag_vm_resource.rmem -= rmem; 
	ppag->ppag_vm_resource.smem -= smem; 

	vm_pool = ppag->ppag_vm_resource.vm_pool;

	unreservemem(vm_pool, smem, rmem, 0);

	mp_mutex_spinunlock(&ppag->ppag_lock, s);
}

/*
 * ppag_set_vm_resource_limits:
 * Set the maximum smem/rmem limits for a given miser job.
 */
void
ppag_set_vm_resource_limits(bhv_desc_t *bhv, pgno_t rss_limit)
{
	ppag_t  *ppag = BHV_TO_PPAG(bhv);

	int s;

        s = mp_mutex_spinlock(&ppag->ppag_lock);

	ppag->ppag_vm_resource.rss_limit = rss_limit;

	mp_mutex_spinunlock(&ppag->ppag_lock, s);

}

/*
 * ppag_get_vm_resource_limits:
 * Set the maximum smem/rmem limits for a given miser job.
 */
void
ppag_get_vm_resource_limits(bhv_desc_t *bhv, pgno_t *rss_limit)
{
	ppag_t  *ppag = BHV_TO_PPAG(bhv);

	int s;

        s = mp_mutex_spinlock(&ppag->ppag_lock);

	*rss_limit = ppag->ppag_vm_resource.rss_limit;

	mp_mutex_spinunlock(&ppag->ppag_lock, s);

}

/*
 * ppag_get_type:
 * Returns the type of process aggregate. There are only two types.
 * array session and miser.
 */
vpag_type_t
ppag_get_type(bhv_desc_t *bhv)
{
	ppag_t *ppag = BHV_TO_PPAG(bhv);
	return ppag->ppag_type;
}

/*
 * ppag_transfer_vm_pool:
 * Transfer a miser job from one pool to another. This can happen
 * for example when a batch job goes critical. At that time it will move
 * from global pool to miser pool. Wakeup any suspended processes.
 */
int
ppag_transfer_vm_pool(bhv_desc_t *bhv, vm_pool_t *new_pool)
{
	ppag_t *ppag = BHV_TO_PPAG(bhv);

	pgno_t 		smem_required, rmem_required;
	int		s;
	vm_pool_t	*old_pool;

	s = mp_mutex_spinlock(&ppag->ppag_lock);

	ASSERT(new_pool != GLOBAL_POOL);
	smem_required = ppag->ppag_vm_resource.smem;
	rmem_required = ppag->ppag_vm_resource.rmem;

	old_pool = ppag->ppag_vm_resource.vm_pool;

	if (old_pool == new_pool) {
		mp_mutex_spinunlock(&ppag->ppag_lock, s);
		return 0;
	};
		

	ASSERT(new_pool->pl_availrmem >= ppag->ppag_vm_resource.rss_limit);

	if (reservemem(new_pool, smem_required, rmem_required, 0)) {
		cmn_err(CE_NOTE,
			"Transfer to pool  0x%x failed smem %d rmem %d\n",
				new_pool, smem_required, rmem_required);
		if (ppag->ppag_flag & PPAG_SUSPEND) {
			ppag->ppag_flag &= ~PPAG_SUSPEND;
			sv_broadcast(&ppag->ppag_wait);
		}
		mp_mutex_spinunlock(&ppag->ppag_lock, s);
		return ENOMEM;
	}

	unreservemem(old_pool, smem_required, rmem_required, 0);
	ppag->ppag_vm_resource.vm_pool = new_pool;

	if (ppag->ppag_flag & PPAG_SUSPEND) {
		ppag->ppag_flag &= ~PPAG_SUSPEND;
		sv_broadcast(&ppag->ppag_wait);
	}

	mp_mutex_spinunlock(&ppag->ppag_lock, s);
	
	return 0;
}

/*
 * ppag_suspend:
 * Suspend the process belonging to a given vpagg. Used by miser 
 * to suspend processes which are in batch mode.
 */
int
ppag_suspend(bhv_desc_t *bhv)
{
	ppag_t *ppag = BHV_TO_PPAG(bhv);
	int	s;

	s = mp_mutex_spinlock(&ppag->ppag_lock);

	ppag->ppag_flag |= PPAG_SUSPEND;

	if (sv_wait_sig(&ppag->ppag_wait, 
			PZERO|PLTWAIT,  &ppag->ppag_lock, s) < 0)
		return 1;
	else 
		return 0;
}

/*
 * ppag_resume:
 * Resume the processes belonging to a given vpagg. Used by miser 
 * to resume processes that were suspend in batch mode and have now
 * become batch critical.
 */
void
ppag_resume(bhv_desc_t *bhv)
{
	ppag_t *ppag = BHV_TO_PPAG(bhv);
	int	s;

	s = mp_mutex_spinlock(&ppag->ppag_lock);
	if (ppag->ppag_flag & PPAG_SUSPEND) {
		ppag->ppag_flag &= ~PPAG_SUSPEND;
		sv_broadcast(&ppag->ppag_wait);
	}
	mp_mutex_spinunlock(&ppag->ppag_lock, s);
}
