/*
 * Handle metrics for cluster router (38)
 *
 */

#ident "$Id: hwrouter.c,v 1.26 1998/11/04 23:24:34 tes Exp $"

#if defined(IRIX6_2) || defined(IRIX6_3)

#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"


/*ARGSUSED*/
void
hwrouter_init(int reset)
{
}

void
hwrouter_fetch_setup(void)
{
}

static pmDesc dummy = {
    0, PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0}
};
    
static pmDesc countzero = {
    0, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0}
};

int
hwrouter_desc(pmID pmid, pmDesc *desc)
{
    switch (pmid) {
    case PMID(1,38,11):	/* hinv.nrouterport */
    case PMID(1,38,0):	/* hinv.nrouter */
	*desc = countzero;
	break;
    default:
	*desc = dummy;
	break;
    }
    desc->pmid = pmid;
    return 0;
}

/*ARGSUSED*/
int
hwrouter_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    pmAtomValue	av;
    int err = 0;

    switch (pmid) {
    case PMID(1,38,0):	/* hinv.nrouter */
    case PMID(1,38,11):	/* hinv.nrouterport */
	vpcb->p_nval = 1;
	sizepool(vpcb);
	av.ul = 0;
	vpcb->p_vp[0].inst = PM_IN_NULL;
	err = __pmStuffValue(&av, 0, vpcb->p_vp, countzero.type);
	vpcb->p_valfmt = err;
	break;
    default:
	/* not supported prior to 6.4 */
	vpcb->p_nval = PM_ERR_APPVERSION;
	break;
    }
    return err;
}

#else /* 6.4 or later */

#if !defined(IRIX6_4)
#define SN0	1
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/sysinfo.h>

#if defined(IRIX6_4)

#include <sys/SN0/slotnum.h>

#else /* IRIX6_5 and later ... */

#include <sys/SN/slotnum.h>

#endif

#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./hwrouter.h"
#include "./xpc.h"

/* descriptors for metrics below hw.router */
static pmDesc meta[] = {
    /* hinv.nrouter */
    { PMID(1,38,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* hinv.map.router */
    { PMID(1,38,1), PM_TYPE_STRING, PM_INDOM_ROUTER, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* hw.router.portmask */
    { PMID(1,38,2), PM_TYPE_U32, PM_INDOM_ROUTER, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* hw.router.rev_id */
    { PMID(1,38,3), PM_TYPE_U32, PM_INDOM_ROUTER, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* hw.router.send_util */
    { PMID(1,38,4), PM_TYPE_U32, PM_INDOM_ROUTER, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* hw.router.recv.bypass_util */
    { PMID(1,38,5), PM_TYPE_U32, PM_INDOM_ROUTER, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* hw.router.recv.queued_util */
    { PMID(1,38,6), PM_TYPE_U32, PM_INDOM_ROUTER, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* hw.router.recv.total_util */
    { PMID(1,38,7), PM_TYPE_U32, PM_INDOM_ROUTER, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* hw.router.retry_errors */
    { PMID(1,38,8), PM_TYPE_U64, PM_INDOM_ROUTER, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },

    /* hw.router.sn_errors */
    { PMID(1,38,9), PM_TYPE_U64, PM_INDOM_ROUTER, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },

    /* hw.router.cb_errors */
    { PMID(1,38,10), PM_TYPE_U64, PM_INDOM_ROUTER, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },

#define LAST_ROUTER_PMID PMID(1,38,10)

    /* hinv.nrouterport */
    { PMID(1,38,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* hinv.map.routerport */
    { PMID(1,38,12), PM_TYPE_STRING, PM_INDOM_ROUTERPORT, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* hinv.interconnect */
    { PMID(1,38,13), PM_TYPE_STRING, PM_INDOM_ROUTERPORT, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* hw.router.perport.send_util */
    { PMID(1,38,14), PM_TYPE_U32, PM_INDOM_ROUTERPORT, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* hw.router.perport.recv.bypass_util */
    { PMID(1,38,15), PM_TYPE_U32, PM_INDOM_ROUTERPORT, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* hw.router.perport.recv.queued_util */
    { PMID(1,38,16), PM_TYPE_U32, PM_INDOM_ROUTERPORT, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* hw.router.perport.recv.total_util */
    { PMID(1,38,17), PM_TYPE_U32, PM_INDOM_ROUTERPORT, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* hw.router.perport.retry_errors */
    { PMID(1,38,18), PM_TYPE_32, PM_INDOM_ROUTERPORT, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },

    /* hw.router.perport.sn_errors */
    { PMID(1,38,19), PM_TYPE_32, PM_INDOM_ROUTERPORT, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },

    /* hw.router.perport.cb_errors */
    { PMID(1,38,20), PM_TYPE_32, PM_INDOM_ROUTERPORT, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },

    /* hw.router.perport.excess_errors */
    { PMID(1,38,21), PM_TYPE_U32, PM_INDOM_ROUTERPORT, PM_SEM_INSTANT, {0,0,0,0,0,0} },

#define NEXT_ROUTER_PMID PMID(1,38,22)
    /* hw.router.type */
    { PMID(1,38,22), PM_TYPE_U32, PM_INDOM_ROUTER, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		direct_map = 1;

int			n_routers = 0;
_pm_router		*routers = (_pm_router *)0;

int			n_routerports = 0;
_pm_router_port		*routerports = (_pm_router_port *)0;

typedef struct {
    unsigned int	module:16;
    unsigned int	slot:4;
    unsigned int	type:4;
    unsigned int	port:4;
    unsigned int	pad:4;
} _pm_router_id;

static int
refresh_hwrouter_stats(_pm_router *r)
{
    int fd;
    int newsize;
    int err = 0;
    char hwgname[MAXPATHLEN];

    sprintf(hwgname, "%s/mon", r->hwgprefix);
    if ((fd = open(hwgname, O_RDONLY)) < 0) {
	err = -errno;
    }
    else {
	if ((newsize = ioctl(fd, GETSTRUCTSIZE)) < 0)  {
	    err = -errno;
	}
	else {
	    if (newsize != r->stats_size || r->stats == (router_info_t *)0) {
		r->stats_size = newsize;
		r->stats = (router_info_t *)realloc(r->stats, r->stats_size);
	    }

	    if (ioctl(fd, ROUTERINFO_GETINFO, r->stats) < 0) {
		err = -errno;
	    }
	}
	close(fd);
    }

    return err;
}

int
reload_hwrouters(void)
{
    /* walk /hw and find all routers */
    char		*cmd = "ls -1 /hw/module/*/slot/r*/*router/mon 2> /dev/null";
    char		buf[MAXPATHLEN];
    char		lbuf[MAXPATHLEN];
    FILE		*fp;
    char		*p;
    _pm_router		*r;
    _pm_router_port	*rp;
    _pm_router_id	id;
    int			i;
    int			port;
    int			routerid;
    int			module;
    int			slot;
    int			j;

    if (routers != (_pm_router *)0) {
	for (i=0; i < n_routers; i++) {
	    if (routers[i].hwgprefix != (char *)0)
		free(routers[i].hwgprefix);
	    if (routers[i].stats != NULL)
		free(routers[i].stats);
	    if (routers[i].name != NULL)
		free(routers[i].name);
	}
    }

    if (routerports != (_pm_router_port *)0) {
	for (i=0; i < n_routers; i++) {
	    if (routerports[i].hwgsuffix != (char *)0)
		free(routerports[i].hwgsuffix);
	    if (routerports[i].hwglink != (char *)0)
		free(routerports[i].hwglink);
	    if (routerports[i].name != (char *)0)
		free(routerports[i].name);
	}
    }

    /* start witha clean slate */
    n_routers = 0;

    if ((fp = popen(cmd, "r")) != NULL) {
	while (fgets(buf, sizeof(buf), fp) != NULL) {
	    n_routers++;
	    routers = (_pm_router *)realloc(routers, n_routers * sizeof(_pm_router));
	    r = &routers[n_routers-1];
	    memset(r, 0, sizeof(_pm_router));
	    if ((p = strrchr(buf, '/')) != NULL)
		*p = NULL;
	    r->hwgprefix = strdup(buf);
	    refresh_hwrouter_stats(r);

	    if (r->stats->ri_version > ROUTER_INFO_VERSION) {
		__pmNotifyErr(LOG_ERR, "reload_hwrouters() - struct router_info_t in kernel (v%d) is newer than in libirixpmda (v%d)\n",
		    (int)r->stats->ri_version, (int)ROUTER_INFO_VERSION);
		n_routers = 0;
		return n_routers;
	    }

	    r->name = __hwg2inst(PM_INDOM_ROUTER, r->hwgprefix);
	    id.module = r->stats->ri_module;
	    id.slot = r->stats->ri_slotnum;
	    id.type = (r->name[0] == 'm');
	    id.port = 0;
	    r->id = *(int *)&id;

#ifdef PCP_DEBUG
	    if (pmIrixDebug & DBG_IRIX_INDOM) {
		__pmNotifyErr(LOG_DEBUG, 
			      "reload_routers: id=0x%08x (%d) \"%s\" module=%d slot=%d type=%d\n",
		    r->id, r->id, r->name, r->stats->ri_module, r->stats->ri_slotnum & SLOTNUM_SLOT_MASK, (r->name[0] == 'm'));
	    }
#endif
	}

	pclose(fp);
    }

    routerports = (_pm_router_port *)realloc(routerports, n_routers * MAX_ROUTER_PORTS * sizeof(_pm_router_port));
    memset(routerports, 0, n_routers * MAX_ROUTER_PORTS * sizeof(_pm_router_port));

    /* set up the list of router ports */
    for (n_routerports=0, i=0; i < n_routers; i++) {

	r = &routers[i];
	refresh_hwrouter_stats(r);

	for (port=0; port < MAX_ROUTER_PORTS; port++) {

	    if (!(r->stats->ri_portmask & (1 << port))) {
		/* inactive, excluded from instance domain */
		continue;
	    }
	    else {
		/*
		 * port is active, (i.e. included in instance domain)
		 * and initially included in the instance profile
		 */
		rp = &routerports[n_routerports];

		rp->active = rp->inc = 1;
		rp->routerindex = i;
		rp->portindex = port;
		id = *(_pm_router_id *)&(r->id);
		id.port = port + 1;
		rp->id = *(int *)&id;

		sprintf(buf, "%d", port+1);
		rp->hwgsuffix = strdup(buf);

		sprintf(buf, "%s/%s", r->hwgprefix, rp->hwgsuffix);
		rp->name = __hwg2inst(PM_INDOM_ROUTERPORT, buf);

		if (rp->name == (char *)0) {
		    free(rp->hwgsuffix);
		    rp->hwgsuffix = NULL;
		    continue;
		}
		    
		if ((p = realpath(buf, lbuf)) != (char *)0) {

		    /*
		     * The router port in the hwg is a symbolic link
		     * which points to the destination node or router.
		     * If this link is not resolved, ignore this port.
		     */
		    rp->hwglink = __hwg2inst(PM_INDOM_NULL, lbuf); /* node or router */

		    if (rp->hwglink[0] == 'm') {	
			j = sscanf(rp->hwglink, "mrouter:%d.%d", &module, &slot);
			if (j == 2) {
			    id.module = module;
			    id.slot = slot;
			    id.type = 1;
			    id.port = 0;
			    routerid = *(int *)&id;
			}
		    }
		    else {
			j = sscanf(rp->hwglink, "router:%d.%d", &module, &slot);
			if (j == 2) {
			    id.module = module;
			    id.slot = slot;
			    id.type = 0;
			    id.port = 0;
			    routerid = *(int *)&id;
			}
		    }

		    if (j == 2) {
			for (j=0; j < n_routers; j++) {
			    if (routerid == routers[j].id)
				break;
			}
			if (j == n_routers) {
			    __pmNotifyErr(LOG_WARNING, 
					  "router in module %d, slot %d is possibly headless (id=%d), ignored.\n", 
					  module, slot, routerid);
			    free(rp->hwgsuffix);
			    rp->hwgsuffix = NULL;
			    continue;
			}
			else
			    n_routerports++;
		    }
		    else
			n_routerports++;

#ifdef PCP_DEBUG
		    if (pmIrixDebug & DBG_IRIX_INDOM) {
			__pmNotifyErr(LOG_DEBUG, "reload_routers: router id=0x%08x \"%s\", port id=0x%08x \"%s\"\n",
			r->id, r->name, rp->id, rp->name);
		    }
#endif
		}
		else {
		    free(rp->hwgsuffix);
		    rp->hwgsuffix = NULL;
		}
	    }
	}
    }

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_INDOM && n_routerports == 0)
	__pmNotifyErr(LOG_WARNING, "reload_hwrouters() - no active hardware routers on this system");
#endif

    return n_routerports;
}

void
hwrouter_init(int reset)
{
    int		i;
    int		err;
    extern int	errno;

    if (reset) {
	/* note: reload_hwrouters() is initially called in indom_init() */
	err = reload_hwrouters();

	if (err < 0) {
	    __pmNotifyErr(LOG_WARNING, 
			 "hwrouter_init: Error: %s\n", pmErrStr(err));
	    nmeta = 0;
	}
    }

    for (i = 0; i < nmeta; i++) {
	if (meta[i].indom != PM_INDOM_NULL) {
	    if (meta[i].pmid <= LAST_ROUTER_PMID ||
		meta[i].pmid >= NEXT_ROUTER_PMID)
		meta[i].indom = indomtab[PM_INDOM_ROUTER].i_indom;
	    else
		meta[i].indom = indomtab[PM_INDOM_ROUTERPORT].i_indom;
	}
	if (direct_map && meta[i].pmid != PMID(1,38,i)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "hwrouterinit: direct map disabled @ meta[%d]", i);
	}

    }
}

void
hwrouter_fetch_setup(void)
{
    int i;

    for (i=0; i < n_routers; i++) {
	routers[i].fetched = 0;
    }
}

int
hwrouter_desc(pmID pmid, pmDesc *desc)
{
    int		i;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].pmid) {
	    *desc = meta[i];	/* struct assignment */
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

/*
 * router metrics
 */
static int
do_hwrouter_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		d;
    int		sts;
    int		nval;
    int		port;
    int		navg;
    pmAtomValue	av;
    __uint64_t	*xpcval;
    _pm_router	*r;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item;
	if (i < nmeta && pmid == meta[i].pmid)
	    goto doit_1;

	__pmNotifyErr(LOG_WARNING, 
		     "do_hwrouter_fetch: direct mapping failed for %s (!= %s)\n",
		     pmIDStr(pmid), pmIDStr(meta[i].pmid));
	direct_map = 0;
    }

    for (i = 0; i < nmeta; i++)
	if (pmid == meta[i].pmid)
	    break;

    if (i >= nmeta) {
	vpcb->p_nval = 0;
	return PM_ERR_PMID;
    }

 doit_1:

    /*
     * Iterate over all the router instances
     * and exclude those instances not included by the profile.
     * Only fetch these stats if required
     */


    for (d=0; d < indomtab[PM_INDOM_ROUTER].i_numinst; d++) {
	r = &routers[d];
	r->inc = __pmInProfile(indomtab[PM_INDOM_ROUTER].i_indom, profp, r->id);
	if (r->inc && r->fetched == 0 && pmid <= LAST_ROUTER_PMID) {
	    refresh_hwrouter_stats(r);
	    r->fetched = 1;
	}
    }
    
    vpcb->p_nval = 0;
    for (d=0; d < indomtab[PM_INDOM_ROUTER].i_numinst; d++) {
	if (routers[d].inc)
	    vpcb->p_nval++;
    }
    sizepool(vpcb);
    
    nval = 0;
    for (d=0; d < indomtab[PM_INDOM_ROUTER].i_numinst; d++) {

	r = &routers[d];

	if (r->inc == 0 ||
	    (r->fetched == 0 && pmid <= LAST_ROUTER_PMID)) {
	    /* not in profile or not active */
	    continue;
	}

	/* special case ones? */
	switch (pmid) {

	case PMID(1,38,1):		/* hw.map.router */
	    /* string value */
	    av.cp = r->hwgprefix;
	    break;

	case PMID(1,38,2):		/* hw.router.portmask */
	    av.ul = r->stats->ri_portmask;
	    break;

	case PMID(1,38,3):		/* hw.router.rev_id */
	    av.ull = r->stats->ri_stat_rev_id;
	    break;

	case PMID(1,38,4):		/* hw.router.send_util */
	    for (navg=0, av.ul=0, port=0; port < MAX_ROUTER_PORTS; port++) {
		if (r->stats->ri_portmask & (1 << port)) {
		    av.ul += r->stats->ri_port[port].rp_util[RP_SEND_UTIL];
		    navg++;
		}
	    }
	    if (navg)
		av.ul = av.ul * 100 / (navg * RR_UTIL_SCALE);
	    else
		av.ul = 0;
	    break;

	case PMID(1,38,5):		/* hw.router.recv.bypass_util */
	    for (navg=0, av.ul=0, port=0; port < MAX_ROUTER_PORTS; port++) {
		if (r->stats->ri_portmask & (1 << port)) {
		    av.ul += r->stats->ri_port[port].rp_util[RP_BYPASS_UTIL];
		    navg++;
		}
	    }
	    if (navg)
		av.ul = av.ul * 100 / (navg * RR_UTIL_SCALE);
	    else
		av.ul = 0;
	    break;

	case PMID(1,38,6):		/* hw.router.recv.queued_util */
	    for (navg=0, av.ul=0, port=0; port < MAX_ROUTER_PORTS; port++) {
		if (r->stats->ri_portmask & (1 << port)) {
		    av.ul += r->stats->ri_port[port].rp_util[RP_RCV_UTIL] - r->stats->ri_port[port].rp_util[RP_BYPASS_UTIL];
		    navg++;
		}
	    }
	    if (navg)
		av.ul = av.ul * 100 / (navg * RR_UTIL_SCALE);
	    else
		av.ul = 0;
	    break;

	case PMID(1,38,7):		/* hw.router.recv.total_util */
	    for (navg=0, av.ul=0, port=0; port < MAX_ROUTER_PORTS; port++) {
		if (r->stats->ri_portmask & (1 << port)) {
		    av.ul += r->stats->ri_port[port].rp_util[RP_RCV_UTIL];
		    navg++;
		}
	    }
	    if (navg)
		av.ul = av.ul * 100 / (navg * RR_UTIL_SCALE);
	    else
		av.ul = 0;
	    break;

	case PMID(1,38,8):		/* hw.router.retry_errors */
	    for (av.ull=0, port=0; port < MAX_ROUTER_PORTS; port++) {
		if (r->stats->ri_portmask & (1 << port)) {
		    xpcval = XPCincr(PMID(1,38,12), r->id | (1<<(port+1)), r->stats->ri_port[port].rp_retry_errors);
		    av.ull += *xpcval;
		}
	    }
	    break;

	case PMID(1,38,9):		/* hw.router.sn_errors */
	    for (av.ull=0, port=0; port < MAX_ROUTER_PORTS; port++) {
		if (r->stats->ri_portmask & (1 << port)) {
		    xpcval = XPCincr(PMID(1,38,13), r->id | (1<<(port+1)), r->stats->ri_port[port].rp_sn_errors);
		    av.ull += *xpcval;
		}
	    }
	    break;

	case PMID(1,38,10):		/* hw.router.cb_errors */
	    for (av.ull=0, port=0; port < MAX_ROUTER_PORTS; port++) {
		if (r->stats->ri_portmask & (1 << port)) {
		    xpcval = XPCincr(PMID(1,38,14), r->id | (1<<(port+1)), r->stats->ri_port[port].rp_cb_errors);
		    av.ull += *xpcval;
		}
	    }
	    break;
	case PMID(1,38,22):		/* hw.router.type */
	    if (r->name[0] == 'm')
		av.ul = 1;
	    else
		av.ul = 0;
	}

	if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].type)) < 0)
	    return sts;
	vpcb->p_vp[nval].inst = r->id;
	if (nval == 0)
	    vpcb->p_valfmt = sts;
	nval++;
    }
    return 0;
}

/*
 * routerport metrics
 */
static int
do_hwrouterport_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		d;
    int		sts;
    int		nval;
    pmAtomValue	av;
    _pm_router	*r;
    _pm_router_port *rp;
    router_port_info_t *rpstats;
    char	sbuf[MAXPATHLEN];

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item;
	if (i < nmeta && pmid == meta[i].pmid)
	    goto doit_2;

	__pmNotifyErr(LOG_WARNING, 
		     "do_hwrouterport_fetch: direct mapping failed for %s (!= %s)\n",
		     pmIDStr(pmid), pmIDStr(meta[i].pmid));
	direct_map = 0;
    }

    for (i = 0; i < nmeta; i++)
	if (pmid == meta[i].pmid)
	    break;

    if (i >= nmeta) {
	vpcb->p_nval = 0;
	return PM_ERR_PMID;
    }

 doit_2:

    /*
     * Iterate over all the router port instances
     * and exclude those instances not included by the profile.
     */
    for (d=0; d < indomtab[PM_INDOM_ROUTERPORT].i_numinst; d++) {
	rp = &routerports[d];
	r = &routers[rp->routerindex];
	if (rp->active) {
	    rp->inc = __pmInProfile(indomtab[PM_INDOM_ROUTERPORT].i_indom, profp, rp->id);
	    if (rp->inc && r->fetched == 0) {
		refresh_hwrouter_stats(r);
		r->fetched = 1;
	    }
	}
    }
    
    vpcb->p_nval = 0;
    for (d=0; d < indomtab[PM_INDOM_ROUTERPORT].i_numinst; d++) {
	rp = &routerports[d];
	if (rp->active && rp->inc)
	    vpcb->p_nval++;
    }
    sizepool(vpcb);
    
    nval = 0;
    for (d=0; d < indomtab[PM_INDOM_ROUTERPORT].i_numinst; d++) {

	rp = &routerports[d];
	r = &routers[rp->routerindex];

	if (r->fetched == 0 || rp->inc == 0 || rp->active == 0) {
	    /* not in profile or not active */
	    continue;
	}

	rpstats = &r->stats->ri_port[rp->portindex];

	/* special case ones? */
	switch (pmid) {

	case PMID(1,38,12):		/* hw.map.routerport */
	    /* string value */
	    sprintf(sbuf, "%s/%s", r->hwgprefix, rp->hwgsuffix);
	    av.cp = sbuf;
	    break;

	case PMID(1,38,13):		/* hinv.interconnect */
	    /* string value has already been converted by __hwg2inst() */
	    av.cp = rp->hwglink; 
	    break;

	case PMID(1,38,14):		/* hw.router.perport.send_util */
	    av.ul = 100 * (int)rpstats->rp_util[RP_SEND_UTIL] / RR_UTIL_SCALE;
	    break;

	case PMID(1,38,15):		/* hw.router.perport.recv.bypass_util */
	    av.ul = 100 * (int)rpstats->rp_util[RP_BYPASS_UTIL] / RR_UTIL_SCALE;
	    break;

	case PMID(1,38,16):		/* hw.router.perport.recv.queued_util */
	    av.ul = 100 * (int)(rpstats->rp_util[RP_RCV_UTIL] - rpstats->rp_util[RP_BYPASS_UTIL]) / RR_UTIL_SCALE;
	    break;

	case PMID(1,38,17):		/* hw.router.perport.recv.total_util */
	    av.ul = 100 * (int)rpstats->rp_util[RP_RCV_UTIL] / RR_UTIL_SCALE;
	    break;

	case PMID(1,38,18):		/* hw.router.perport.retry_errors */
	    av.l = rpstats->rp_retry_errors;
	    break;

	case PMID(1,38,19):		/* hw.router.perport.sn_errors */
	    av.l = rpstats->rp_sn_errors;
	    break;

	case PMID(1,38,20):		/* hw.router.perport.cb_errors */
	    av.l = rpstats->rp_cb_errors;
	    break;

	case PMID(1,38,21):		/* hw.router.perport.excess_errors */
	    av.ul = rpstats->rp_excess_err;
	    break;
	}

	if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].type)) < 0)
	    return sts;
	vpcb->p_vp[nval].inst = rp->id;
	if (nval == 0)
	    vpcb->p_valfmt = sts;
	nval++;
    }
    return 0;
}

int
hwrouter_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		err;
    int		i;
    pmAtomValue	av;

    switch (pmid) {
    case PMID(1,38,0):		/* hinv.nrouter */
    case PMID(1,38,11):		/* hinv.nrouterport */
	/*
	 * singular instance domain
	 */

	if (direct_map) {
	    __pmID_int       *pmidp = (__pmID_int *)&pmid;
	    i = pmidp->item;
	    if (i < nmeta && pmid == meta[i].pmid)
		goto doit_3;

	    __pmNotifyErr(LOG_WARNING, 
			 "hwrouter_fetch: direct mapping failed for %s (!= %s)\n",
			 pmIDStr(pmid), pmIDStr(meta[i].pmid));
	    direct_map = 0;
	}
	
	for (i = 0; i < nmeta; i++)
	    if (pmid == meta[i].pmid)
		break;

    doit_3:

	if (pmid == PMID(1,38,0))
	    av.ul = n_routers;
	else if (pmid == PMID(1,38,11))
	    av.ul = n_routerports;

	vpcb->p_nval = 1;
	vpcb->p_vp[0].inst = PM_IN_NULL;
	err = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].type);
	vpcb->p_valfmt = err;
	
	break;

    default:
	if (pmid <= LAST_ROUTER_PMID ||
	    pmid >= NEXT_ROUTER_PMID)
	    /* router instance domain */
	    err = do_hwrouter_fetch(pmid, profp, vpcb);
	else
	    /* routerport instance domain */
	    err = do_hwrouterport_fetch(pmid, profp, vpcb);
	break;
    }
    
    return err;
}
#endif /* defined(IRIX6_2) || defined(IRIX6_3) */

