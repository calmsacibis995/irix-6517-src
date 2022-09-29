/*
 * Handle metrics for R10000K event counter cluster (35)
 */

#ident "$Id: evctr.c,v 1.15 1999/10/14 07:21:40 tes Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <procfs/procfs.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/syssgi.h>
#include <sys/systeminfo.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

/*
 * every counter metric has the same pmDesc
 */
static pmDesc		thedesc =
  { PMID(1,35,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} };

static pmDesc		statedesc =
  { PMID(1,35,255), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} };

static pmDesc		cpurevdesc =
  { PMID(1,35,254), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} };

#ifndef HWPERF_EVENTMAX
#define HWPERF_EVENTMAX 32
#endif

/*
 * Counter 14 has different interpretation, depending on the version of
 * the R10000 CPU ... see comments before get_r10k_version() below
 *
 */
#define ITEM_VCC		14
#define ITEM_FUCOMP		32
#define MAX_ITEM		32

static int		Active[HWPERF_EVENTMAX];
static int		Mux[HWPERF_EVENTMAX];
static __uint64_t	Count[HWPERF_EVENTMAX];
static int		support = 0;		/* any chance? */
static int		enabled = 1;		/* ecadmin run to enable counters? */
static int		r10k_version = 0;	/* encoded as maj*100 + min */
static char		cpurev[8] = "unknown";

/*
 * The following code is stolen from SpeedShop/prof/program.cxx, which
 * in turn notes that the following cpu/fpu code is stolen from
 * irix/cmd/hinv.c
 *
 * coprocessor revision identifiers
 */

#include <invent.h>

typedef union rev_id {
        unsigned int    ri_uint;
        struct {
#ifdef MIPSEB
                unsigned int    Ri_fill:16,
                                Ri_imp:8,               /* implementation id */
                                Ri_majrev:4,            /* major revision */
                                Ri_minrev:4;            /* minor revision */
#endif
#ifdef MIPSEL
                unsigned int    Ri_minrev:4,            /* minor revision */
                                Ri_majrev:4,            /* major revision */
                                Ri_imp:8,               /* implementation id */
                                Ri_fill:16;
#endif
        } Ri;
} rev_id_t;

#define ri_imp          Ri.Ri_imp
#define ri_majrev       Ri.Ri_majrev
#define ri_minrev       Ri.Ri_minrev

void
get_r10k_version(int *major, int *minor)
{
    inventory_t	*invp;
    rev_id_t	*revp;

    setinvent();
    while ((invp = getinvent()) != NULL) {
	if (invp->inv_class == INV_PROCESSOR && invp->inv_type == INV_CPUCHIP) {
	    revp = (rev_id_t *)&invp->inv_state;
	    if (revp->ri_imp == 0) {
		*major = 1;
		*minor = 5;
	    }
	    else {
		*major = revp->ri_majrev;
		*minor = revp->ri_minrev;
	    }
	    sprintf(cpurev, "%d.%d", revp->ri_majrev, revp->ri_minrev);
	    endinvent();
	    return;
	}
    }
    *major = -1;
    *minor = -1;
    endinvent();
}

void
evctr_init(int reset)
{
    char	buf[257];	/* see sysinfo(2) for suggested maximum */
    char	*p;
    int		major;
    int		minor;

    if (reset)
	return;

    if (sysinfo(_MIPS_SI_PROCESSORS, buf, sizeof(buf)) < 0)
	return;

    /* find end of first word */
    for (p = buf; *p; p++) {
	if (*p == ' ') {
	    *p = '\0';
	    break;
	}
    }

    if (strcmp(buf, "R10000") != 0)
	return;
  
    /* Hw counter change in R10K from rev 2.x to rev 3.x !!
     *
     ****     14 = Virtual coherency conditions
     * In rev 2.x this counter is incremented on the cycle after a virtual
     *   address coherence condition is detected, provided that the
     *   access was not flagged as a miss.  This condition can only be
     *   realized for virtual page sizes of 4KB.
     *  
     ****     14 = Functional unit completion cycles
     In rev 3.x this counter's meaning is changed.  It is incremented on
     *   the cycle after either ALU1, ALU2, FPU1, or FPU2 marks an
     *   instruction as "done."
     ****
     */

    get_r10k_version(&major, &minor);
    if (major != -1)
	r10k_version = major * 100 + minor;

    support = 1;
}

static void
warn(void)
{
    if (enabled) {
	__pmNotifyErr(LOG_WARNING, "R10K event counters are disabled ...\nUse of ecadmin(1) may be needed to activate system-level event counters\n");
	enabled = 0;
    }
}

static void
ok(void)
{
    if (!enabled) {
	__pmNotifyErr(LOG_WARNING, "R10K event counters re-enabled ...\n");
	enabled = 1;
    }
}

void
evctr_fetch_setup(void)
{
    int			ctr;
    long		sts;
    int			use[2];		/* only 2 real registers on T5 */
    static long		gen = -1;	/* generation number */

    if (!support)
	return;

    /* retrieve the counts */
    if ((sts = syssgi(SGI_EVENTCTR, HWPERF_GET_SYSCNTRS, (void *)&Count)) < 0) {
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++)
	    Active[ctr] = 0;
	warn();
	return;
    }

    if (sts != gen) {
	hwperf_profevctrarg_t	evctr_args_get;

	/* config changed, get new setup */
	if ((gen = syssgi(SGI_EVENTCTR, HWPERF_GET_SYSEVCTRL, &evctr_args_get)) < 0) {
	    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++)
		Active[ctr] = 0;
	    warn();
	    return;
	}

	use[0] = use[1] = 0;
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
	    if (!evctr_args_get.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec) {
		Active[ctr] = 0;
		continue;
	    }
	    Active[ctr] = 1;
	    if (ctr < HWPERF_CNT1BASE)
		use[0]++;
	    else
		use[1]++;
	}

	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
	    if (Active[ctr]) {
		if (ctr < HWPERF_CNT1BASE)
		    Mux[ctr] = use[0];
		else
		    Mux[ctr] = use[1];
	    }
	    else
		Mux[ctr] = 0;
	}
    }

    ok();
}

int evctr_desc(pmID pmid, pmDesc *desc)
{
    __pmID_int	*pmidp = (__pmID_int *)&pmid;

    if (pmidp->item <= MAX_ITEM && pmidp->item != 16 && pmidp->item != 17) {
	*desc = thedesc;	/* struct assignment */
	desc->pmid = pmid;
	if (!support ||
	    pmidp->item == ITEM_VCC && r10k_version >= 301 ||
	    pmidp->item == ITEM_FUCOMP && r10k_version < 301)
	    desc->type = PM_TYPE_NOSUPPORT;
	return 0;
    }
    else if (pmidp->item == 254) {
	*desc = cpurevdesc;	/* struct assignment */
	return 0;
    }
    else if (pmidp->item == 255) {
	*desc = statedesc;	/* struct assignment */
	return 0;
    }
    return PM_ERR_PMID;
}

/*ARGSUSED*/
int evctr_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		ctr;
    int		sts;
    pmAtomValue	atom;
    __pmID_int	*pmidp = (__pmID_int *)&pmid;

    ctr = pmidp->item;
    if (ctr <= MAX_ITEM && ctr != 16 && ctr != 17) {
	if (!support ||
	    ctr == ITEM_VCC && r10k_version >= 301 ||
	    ctr == ITEM_FUCOMP && r10k_version < 301)
	    /* not an R10K, or counter not on this version */
	    vpcb->p_nval = PM_ERR_APPVERSION;
	else if (!enabled)
	    /* event counters are not enabled at all */
	    vpcb->p_nval = PM_ERR_AGAIN;
	else {
	    /*
	     * handle [0,16] and [15,17] aliases, and [14] aliases
	     * in different chip versions
	     */
	    if (ctr == ITEM_FUCOMP)
		ctr = ITEM_VCC;
	    if (ctr == 0 && Active[ctr] == 0)
		ctr = 16;
	    if (ctr == 15 && Active[ctr] == 0)
		ctr = 17;
	    if (Active[ctr]) {
		vpcb->p_nval = 1;
		vpcb->p_vp[0].inst = PM_IN_NULL;
		atom.ull = Count[ctr] * Mux[ctr];
		if ((sts = __pmStuffValue(&atom, 0, vpcb->p_vp, thedesc.type)) < 0)
		    return sts;
		vpcb->p_valfmt = sts;
	    }
	    else
		/* this event counter is not enabled */
		vpcb->p_nval = PM_ERR_VALUE;
	}
	return vpcb->p_nval;
    }
    else if (ctr == 254) {
	atom.cp = cpurev;
	if ((sts = __pmStuffValue(&atom, 0, vpcb->p_vp, cpurevdesc.type)) < 0)
	    return sts;
	vpcb->p_valfmt = sts;
	return vpcb->p_nval = 1;
    }
    else if (ctr == 255) {
	if (!support)
	    atom.l = -1;
	else if (!enabled)
	    atom.l = 0;
	else {
	    atom.l = 0;
	    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
		if (Active[ctr]) atom.l++;
	    }
	}
	if ((sts = __pmStuffValue(&atom, 0, vpcb->p_vp, statedesc.type)) < 0)
	    return sts;
	vpcb->p_valfmt = sts;
	return vpcb->p_nval = 1;
    }
    return PM_ERR_PMID;
}
