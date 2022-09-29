#if defined(NUMA_REPLICATION)
#ifdef	DEBUG
/*****************************************************************************
 * repl_debug.c
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/

#ident "$Revision: 1.13 $"

#include <sys/types.h>
#include <sys/immu.h>
#include <sys/cmn_err.h>
#include <sys/pda.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <ksys/vproc.h>
#include <sys/systm.h>                  /* splhi        */
#include <sys/sysmp.h>                  /* numainfo     */
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/ktrace.h>
#include <sys/pda.h>
#include "repl_policy.h"
#include "replattr.h"
#include "repldebug.h"

extern void _prsymoff(void *, char *, char *);

ktrace_t        *replshoot_trace;
ktrace_t        *pagerepl_trace;


extern time_t	lbolt;
#define	log_type	val[0]
#define	pslog_vp	val[1]
#define	pslog_first	val[2]
#define	pslog_last	val[3]
#define	pslog_pgrfirst	val[4]
#define	pslog_pgrlast	val[5]
#define	pslog_pgrflags	val[6]
#define	pslog_pgrcnt	val[7]
#define	pslog_vrattrpid	val[8]
#define	pslog_when	val[9]
#define	pslog_time	val[10]


void
repllist_shoot_log(vnode_t *v, pgno_t first, pgno_t last, long when)
{
	long		cur_type;
	long 		pid;
	long		v1, v2, v3, v4;
	replattr_t	*vrattrp  = replattr_lookup(v);
	long	curtime = lbolt;
	
	cur_type = SHOOT_LOG;
	pid      = current_pid();

	ASSERT(vrattrp);

	v1 = v2 = 0;
	v3	 = vrattrp->replstate;
	v4	 = vrattrp->replpgcnt;

	ktrace_enter (replshoot_trace, 
		(void *)cur_type, (void *)v, (void *)first, (void *)last, 
		(void *)v1, (void *)v2, (void *)v3, (void *)v4, 
		(void *)pid, (void *)when, (void *)curtime, 0, 
		0, 0, 0, 0);

}

int
idbg_pshootlog(void *p)
{
	vnode_t	*vp = (vnode_t *)p;
	int	i;
	ktrace_entry_t	*kte = replshoot_trace->kt_entries;

	qprintf("Shoot down Log with vnode = 0x%x\n", (long)vp);
	i = replshoot_trace->kt_index;

	do {
		if ( ((long)(kte + i)->log_type == SHOOT_LOG) &&
		     (  ((kte + i)->pslog_vp && (vp == (vnode_t *)-1L)) ||
		 	((kte + i)->pslog_vp == vp) ) ){

			long when = (long)(kte + i)->pslog_when;

			qprintf(" @%d %s Flag %d pgrcnt %d pid %d ", 
				(kte + i)->pslog_time,
				(when == 0 ? "E" : "X"),
				(kte + i)->pslog_pgrflags,
				(kte + i)->pslog_pgrcnt,
				(kte + i)->pslog_vrattrpid);

			qprintf("Rng %d:%d, vp %x\n",
				(kte + i)->pslog_first,
				(kte + i)->pslog_last,
				(kte + i)->pslog_vp);


		}
		if ( ++i == VROPS_MAX_SLOG)
			i = 0;

	} while (i != replshoot_trace->kt_index);

	return 0;

}


int
idbg_prrepl(void *arg)
{
	vnode_t		*vp;
	replattr_t	*vrattrp;
	repl_policy_t	*rp;

	vp = (vnode_t *)arg;

	if(VN_ISREPLICABLE(vp) || VN_ISNONREPLICABLE(vp)) {
		vrattrp = replattr_lookup(vp);

		if (!vrattrp){
			/* Happens if vnode was marked non replicable first */
			qprintf("Vnode 0x%x has no replicated structure\n", vp);
			return 0;
		}

		qprintf("  Replattr Addr: 0x%x  State %d pagecnt %d\n", vrattrp,
		vrattrp->replstate, vrattrp->replpgcnt);

		rp = REPLPOLICY_GET(vrattrp);

		if (rp) {
			qprintf("  Policy(@%x): pvt: %x\n",
				rp, rp->policy_pvt);

			_prsymoff((void *)rp->update, "  Method:", ", ");
			_prsymoff((void *)rp->getpage, NULL, ", ");
			_prsymoff((void *)rp->free, NULL, "\n");
		}else {
			qprintf(" NO replication policy attached\n");
		}

	}
	else {
		qprintf("vnode %x has no replicated structure \n", vp);
	}

	return 0;
}

/*ARGSUSED*/
int
idbg_replstat(void *arg)
{
	
	qprintf("shoot-down: %d pages-shot: %d ",
		REPL_PAGE_STATS.replshootdn, REPL_PAGE_STATS.replpgshotdn);
	
	qprintf("replobjs: %d\n", REPL_VN_STATS.replobjs);

	qprintf("setup: %d teardn: %d getpage %d \n", 
		REPL_POLICY_STAT.rp_setup,  REPL_POLICY_STAT.rp_teardn,
		REPL_POLICY_STAT.rp_getpage);

	qprintf(" affupdate %d affmatch %d affnonode %d \n",
		REPL_POLICY_STAT.rp_affupdate, REPL_POLICY_STAT.rp_affmatch,
		REPL_POLICY_STAT.rp_affnonode);
	return 0;
}

void
idbg_replfuncs(void)
{
	idbg_addfunc("vnrepl", (void (*)())idbg_prrepl);
	idbg_addfunc("pgrslog", (void (*)())idbg_pshootlog);
	idbg_addfunc("replstat", (void (*)())idbg_replstat);
	idbg_addfunc("replpvt", (void (*)())idbg_replprint);
	/* idbg_addfunc("pgrlist", (void (*)())idbg_pgrlist); */
}

#endif	/* DEBUG */
#endif /* defined(NUMA_REPLICATION) */
