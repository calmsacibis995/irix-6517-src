/*
 * Handle metrics for cluster xbow (40)
 */

#ident "$Id: xbow.c,v 1.16 1999/04/28 02:16:41 tes Exp $"

#if defined(IRIX6_2) || defined(IRIX6_3)

#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

int			n_xbows = 0;	    /* number of xbows in use */
int			n_xbowports = 0;    /* number xbow ports in use */
int			n_xbowportln = 0;   /* length of xbow port array */

/*ARGSUSED*/
void
xbow_init(int reset)
{
}

int
reload_xbows(void)
{
    return 0;
}

int
reload_xbowports(void)
{
    return 0;
}

void
xbow_fetch_setup(void)
{
}

static pmMeta	meta[] = {
/* hinv.nxbow */
 { 0, { PMID(1,42,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* other */
 { 0, { 0, PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }
};

int
xbow_desc(pmID pmid, pmDesc *desc)
{
    if (pmid == PMID(1,42,1)) {		/* hinv.nxbow is always supported */
	*desc = meta[0].m_desc;
	return 0;
    }

    meta[1].m_desc.pmid = pmid;
    *desc = meta[1].m_desc;
    return 0;
}

/*ARGSUSED*/
int
xbow_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		sts = 0;
    pmAtomValue	av;

    if (pmid == PMID(1,42,1)) {		/* hinv.nxbow is always supported */
	av.ul = 0;
	vpcb->p_nval = 1;
	vpcb->p_vp[0].inst = PM_IN_NULL;
	sts = __pmStuffValue(&av, 0, vpcb->p_vp, meta[0].m_desc.type);
	vpcb->p_valfmt = sts;
    }
    else
	vpcb->p_nval = PM_ERR_APPVERSION;

    return sts;
}

/*ARGSUSED*/
int
xbow_store(pmValueSet *vsp)
{
    return -EACCES;
}

#else /* 6.4 or later */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <paths.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./xbow.h"

static xb_vcounter_t xbinfo;

static pmMeta	meta[] = {
/* hinv.nxbow */
 { 0, { PMID(1,42,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* hinv.map.xbow */
 { 0, { PMID(1,42,2), PM_TYPE_STRING, PM_INDOM_XBOW, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* irix.xbow.active.xbows */
 { 0, { PMID(1,42,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* irix.xbow.active.ports */
 { 0, { PMID(1,42,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* irix.xbow.switch */
 { 0, { PMID(1,42,5), PM_TYPE_U32, PM_INDOM_XBOW, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* irix.xbow.nports */
 { 0, { PMID(1,42,6), PM_TYPE_U32, PM_INDOM_XBOW, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* irix.xbow.gen */
 { 0, { PMID(1,42,7), PM_TYPE_U32, PM_INDOM_XBOW, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* irix.xbow.total.src */
  { 0, { PMID(1,42,8), PM_TYPE_U64, PM_INDOM_XBOW, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xbow.total.dst */
  { 0, { PMID(1,42,9), PM_TYPE_U64, PM_INDOM_XBOW, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xbow.total.rrcv */
  { 0, { PMID(1,42,10), PM_TYPE_U64, PM_INDOM_XBOW, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xbow.total.rxmt */
  { 0, { PMID(1,42,11), PM_TYPE_U64, PM_INDOM_XBOW, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xbow.port.flags */
  { (char *)&xbinfo.flags, { PMID(1,42,12), PM_TYPE_U64, PM_INDOM_XBOWPORT, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* irix.xbow.port.src */
  { (char *)&xbinfo.vsrc, { PMID(1,42,13), PM_TYPE_U64, PM_INDOM_XBOWPORT, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xbow.port.dst */
  { (char *)&xbinfo.vdst, { PMID(1,42,14), PM_TYPE_U64, PM_INDOM_XBOWPORT, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xbow.port.rrcv */
  { (char *)&xbinfo.crcv, { PMID(1,42,15), PM_TYPE_U64, PM_INDOM_XBOWPORT, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xbow.port.rxmt */
  { (char *)&xbinfo.cxmt, { PMID(1,42,16), PM_TYPE_U64, PM_INDOM_XBOWPORT, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		direct_map = 1;
static int		xbowSize = 0;
static int		xbowportSize = 0;
static int		n_mappedxbows = 0;

int			n_xbows = 0;	    /* number of xbows in use */
int			n_xbowports = 0;    /* number of xbow ports in use */
int			n_xbowportln = 0;   /* length of xbow port array */

_pm_xbow		*xbows = (_pm_xbow *)0;
_pm_xbowport		*xbowports = (_pm_xbowport *)0;

extern int		errno;

typedef struct {
    unsigned int	module:16;
    unsigned int	slot:8;
    unsigned int	port:8;
} _pm_xbow_id;

int reload_xbows(void);
int reload_xbowports(void);
void xbow_start(int x);
void xbow_stop(int x);

/* ARGSUSED */
void
xbow_init(int reset)
{
    int		i = 0;

    if (reset)
	return;

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_XBOW) {
	if (n_xbows == 0)
	    __pmNotifyErr(LOG_WARNING, "No xbows found in hardware graph\n");
	else
	    __pmNotifyErr(LOG_DEBUG, "n_xbows = %d, n_xbowports = %d (of %d)\n",
			 n_xbows, n_xbowports, n_xbowportln);
    }
#endif

    for (i = 0; i < nmeta; i++) {
	if (meta[i].m_desc.indom == PM_INDOM_XBOW)
	    meta[i].m_desc.indom = indomtab[PM_INDOM_XBOW].i_indom;
	else if (meta[i].m_desc.indom == PM_INDOM_XBOWPORT)
	    meta[i].m_desc.indom = indomtab[PM_INDOM_XBOWPORT].i_indom;
	
	if (meta[i].m_offset != (char *)0)
	    meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - 
						    (char *)&xbinfo));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,42,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			  "xbow_init: direct map disabled @ meta[%d]", i);
	}
    }
}

struct _xbow_state {
    int		id;
    int		mapped;
    int		gen;
};

int
reload_xbows(void)
{
    char		*cmd = "/usr/bin/find " _PATH_HWGFS " -name mon -print | /sbin/grep xtalk";
    char		buf[MAXPATHLEN];
    char		*str = NULL;
    char		*c = NULL;
    FILE		*fp = NULL;
    _pm_xbow		*xb_ptr = NULL;
    _pm_xbowport	*xbp_ptr = NULL;
    _pm_xbow_id		id;
    int			module = 0;
    int			slot = 0;
    int			i = 0;
    int			x = 0;
    int			p = 0;
    int			old_nxbows = 0;
    int			old_nmapped = 0;
    struct _xbow_state	*old_state = NULL;

    if (n_xbows > 0 && xbows != (_pm_xbow *)0) {
    	/* maintain list of xbow ids that were mapped so that they can
	   be mapped again */
    	old_nxbows = n_xbows;
    	old_state = (struct _xbow_state *)malloc(old_nxbows * 
						 sizeof(struct _xbow_state));
	if (old_state == NULL) {
	    __pmNotifyErr(LOG_CRIT, 
			 "reload_xbows: unable to alloc %d bytes for old ids\n",
			 old_nxbows * sizeof(struct _xbow_state));
	    n_xbows = 0;
	    free(xbows);
	    xbowSize = 0;
	    return 0;
	}
	for (i = 0; i < n_xbows; i++) {
	    if (xbows[i].hwgname != (char *)0) {
		old_state[old_nmapped].id = xbows[i].id;
		old_state[old_nmapped].gen = xbows[i].gen;
		old_state[old_nmapped].mapped = 0;
	    	if (xbows[i].mapped) {	/* Stop all xbows that are mapped */
		    old_state[old_nmapped].mapped = 1;
		    xbow_stop(i);
		}
		free(xbows[i].extname);
		free(xbows[i].hwgname);
		old_nmapped++;
	    }
	}
	memset(xbows, 0, n_xbows * sizeof(_pm_xbow));
    }

    if (n_xbowportln > 0 && xbowports != NULL) {
	for (i = 0; i < n_xbowportln; i++) {
	    if (xbowports[i].extname != NULL)
		free(xbowports[i].extname);
	}
	memset(xbowports, 0, xbowportSize * sizeof(_pm_xbowport));
    }

    n_xbows = 0;
    n_xbowports = 0;
    n_xbowportln = 0;

    if ((fp = popen(cmd, "r")) != (FILE *)0) {
	while (fgets(buf, sizeof(buf), fp) != (char *)0) {
	    n_xbows++;
	    if (n_xbows >= xbowSize) {
		xbowSize += 4;
		xbows = (_pm_xbow *)realloc(xbows, 
					    xbowSize * sizeof(_pm_xbow));
		if (xbows == (_pm_xbow *)0) {
		    __pmNotifyErr(LOG_CRIT, 
				 "reload_xbows: unable to alloc %d bytes\n",
				 xbowSize * sizeof(_pm_xbow));
		    n_xbows = 0;
		    xbowSize = 0;
		    break;
		}
	    }
	    
	    xb_ptr = &xbows[n_xbows-1];
	    memset(xb_ptr, 0, sizeof(_pm_xbow));

	    /* use path to xbow in hwgfs as external instance id */
	    if ((c = strrchr(buf, '\n')) != (char *)0)
		*c = '\0';
	    xb_ptr->hwgname = strdup(buf);
	}
	pclose(fp);
    }

    n_xbowportln = n_xbows * (XB_MON_LINK_MASK + 1);
    if (n_xbowportln >= xbowportSize) {
    	xbowportSize = n_xbowportln;
	xbowports = (_pm_xbowport *)realloc(xbowports,
					    xbowportSize * sizeof(_pm_xbowport));
	if (xbowports == NULL) {
	    __pmNotifyErr(LOG_CRIT, 
			 "reload_xbowports: unable to alloc %d bytes\n",
			 xbowportSize * sizeof(_pm_xbowport));
	    n_xbows = 0;
	    xbowSize = 0;
	    free(xbows);
	    n_xbowportln = 0;
	    xbowportSize = 0;
	    return 0;
	}
    }
 
   for (x = 0; x < n_xbows; x++) {
	xb_ptr = &xbows[x];

	str = __hwg2inst(PM_INDOM_XBOW, xb_ptr->hwgname);

	/* could not parse hardware graph */
	if (strcmp(str, buf) == 0) {
#ifdef PCP_DEBUG
	    if (pmIrixDebug & DBG_IRIX_XBOW)
		__pmNotifyErr(LOG_DEBUG,
			     "reload_xbows: Unable to extract xbow location from hardware graph: %s\n",
			     buf);
#endif
	    id.module = 0;
	    id.slot = x;
	    xb_ptr->id = x;
	}
	else {
	    sscanf(str, "xbow:%d.%d", &module, &slot);
	    id.module = module;
	    id.slot = slot;
	    id.port = 0;
	    xb_ptr->id = *(int *)(&id);
	}

	free(str);
	xb_ptr->extname = (char *)malloc(8 * sizeof(char));
	sprintf(xb_ptr->extname, "xbow%d", x);

#ifdef PCP_DEBUG
	if (pmIrixDebug & DBG_IRIX_XBOW) {
	    __pmNotifyErr(LOG_DEBUG,
			 "reload_xbows: Xbow %s(%d) mapped to %s\n",
			 xb_ptr->extname, xb_ptr->id, xb_ptr->hwgname);
	}
#endif

	for (i = 0; i < old_nmapped; i++) {
	    if (xb_ptr->id == old_state[i].id) {
		xb_ptr->mapped = old_state[i].mapped;
		xb_ptr->gen = old_state[i].gen;
	    }
	}

	for (p = 0; p <= XB_MON_LINK_MASK ; p++) {
	    xbp_ptr = XBOWPORT_HNDL(x, p);
	    id.port = p + 8;
	    xbp_ptr->id = *(int *)(&id);
	    xbp_ptr->extname = (char *)malloc(32 * sizeof(char));
	    sprintf(xbp_ptr->extname, "xbowport%d.%X", x, id.port);
	    xbp_ptr->cntrs = NULL;
	}

    }

    if (old_state != NULL)
    	free(old_state);

    return n_xbows;
}

int
reload_xbowports(void)
{
    int			x = 0;

    if (n_xbows == 0 || n_xbowportln == 0)
    	return 0;

    for (x = 0; x < n_xbows; x++) {
	if (xbows[x].mapped && xbows[x].block == NULL)
	    xbow_start(x);
    }

    return n_xbowports;
}

int
xbow_desc(pmID pmid, pmDesc *desc)
{
    int         i;

    for (i = 0; i < nmeta; i++) {
        if (pmid == meta[i].m_desc.pmid) {
            *desc = meta[i].m_desc;     /* struct assignment */
            return 0;
        }
    }
    return PM_ERR_PMID;
}

/* start monitoring an xbows */
void
xbow_start(int x)
{
    _pm_xbow		*xb_ptr = NULL;
    _pm_xbowport	*xbp_ptr = NULL;
    int			p = 0;
    int			sts = 0;
    __uint32_t		xbVersion;

    xb_ptr = &(xbows[x]);
    xb_ptr->block = (xb_vcntr_block_t *)0;
    xb_ptr->nports = 0;

    xb_ptr->devfd = open(xb_ptr->hwgname, O_RDONLY);
    if (xb_ptr->devfd < 0) {
	__pmNotifyErr(LOG_ERR, 
		     "xbow_start: unable to open device %s for %s[%d]: %s\n",
		     xb_ptr->hwgname, xb_ptr->extname, x, strerror(errno));
	return;
    }

    if ((sts = ioctl(xb_ptr->devfd, XB_GET_VERSION, &xbVersion)) < 0) {
	__pmNotifyErr(LOG_ERR,
		     "xbow_start: ioctl(XB_GET_VERSION) failed for %s[%d]: %s\n",
		     xb_ptr->extname, x, strerror(errno));
	close(xb_ptr->devfd);
	xb_ptr->devfd = sts;
	return;
    }

    if (xbVersion != XB_VERSION) {
	__pmNotifyErr(LOG_ERR,
		     "xbow_start: unexpected version (%d != %d) for %s[x]\n",
		     xbVersion, XB_VERSION, xb_ptr->extname, x);
	close(xb_ptr->devfd);
	xb_ptr->devfd = -EPROTO;
	return;
    }

    if ((sts = ioctl(xb_ptr->devfd, XB_ENABLE_MPX_MODE, 0)) < 0) {
	__pmNotifyErr(LOG_ERR,
		     "xbow_start: unable to set mode for %s[%d]: %s\n",
		     xb_ptr->extname, x, strerror(errno));
	close(xb_ptr->devfd);
	xb_ptr->devfd = sts;
	return;
    }

    if ((sts = ioctl(xb_ptr->devfd, XB_START_TIMER, 0)) < 0) {
	__pmNotifyErr(LOG_ERR,
		     "xbow_start: unable to start timer for %s[%d]: %s\n",
		     xb_ptr->extname, x, strerror(errno));
	close(xb_ptr->devfd);
	xb_ptr->devfd = sts;
	return;
    }

    xb_ptr->block = (xb_vcntr_block_t *)mmap(0,
					     sizeof(xb_vcntr_block_t),
					     PROT_READ,
					     MAP_PRIVATE,
					     xb_ptr->devfd,
					     0);

    if (xb_ptr->block == NULL) {
    	__pmNotifyErr(LOG_ERR,
		     "xbow_start: mmap failed for %s[%d]: %s\n",
		     xb_ptr->extname, x, strerror(errno));
	close(xb_ptr->devfd);
	xb_ptr->devfd = errno;
	return;
    }

    for (p = 0; p < XB_MON_LINK_MASK+1; p++) {
    	xbp_ptr = XBOWPORT_HNDL(x,p);
    	xbp_ptr->cntrs = &(xb_ptr->block->vcounters[p]);
	if (XBOWPORT_ACTIVE(xbp_ptr))
	    xb_ptr->nports++;
    }

    n_xbowports += xb_ptr->nports;
    n_mappedxbows++;
    xb_ptr->mapped = 1;
    xb_ptr->gen++;

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_XBOW)
	__pmNotifyErr(LOG_DEBUG,
		     "xbow_start: started %s[%d] OK with %d ports (of %d)\n", 
		     xb_ptr->extname, x, xb_ptr->nports, n_xbowports);
#endif

}

void
xbow_stop(int x)
{
    _pm_xbow	*xb_ptr = (_pm_xbow *)0;
    int 	p;

    xb_ptr = &(xbows[x]);

    if (ioctl(xb_ptr->devfd, XB_STOP_TIMER, 0) < 0) {
	__pmNotifyErr(LOG_ERR,
		     "xbow_start: unable to stop timer for %s[%d]: %s\n",
		     xb_ptr->extname, x, strerror(errno));
    }

    munmap((caddr_t) xb_ptr->block, sizeof(xb_vcntr_block_t));
    xb_ptr->block = (xb_vcntr_block_t *)0;

    close(xb_ptr->devfd);

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_XBOW)
	__pmNotifyErr(LOG_DEBUG, 
		     "xbow_stop: stopped %s[%d]\n", xb_ptr->extname, x);
#endif

    n_xbowports -= xb_ptr->nports;
    n_mappedxbows--;

    xb_ptr->nports = 0;
    xb_ptr->mapped = 0;
    xb_ptr->block = NULL;

    for (p = 0; p < XB_MON_LINK_MASK+1; p++)
    	XBOWPORT_HNDL(x, p)->cntrs = NULL;
}

int
xbow_store(pmValueSet *vsp)
{
    int		i;
    int		x;
    int		sts;
    pmAtomValue	av;	

    for (i = 0; i < vsp->numval; i++) {
	for (x = 0; x < n_xbows; x++)
	    if (XBOW_ID(&xbows[x]) == vsp->vlist[i].inst)
		break;
	if (x == n_xbows)
	    return PM_ERR_INST;
	if ((sts = pmExtractValue(vsp->valfmt, 
				  &vsp->vlist[i], 
				  PM_TYPE_32,
				  &av,
				  PM_TYPE_32)) < 0)
	    return sts;

#ifdef PCP_DEBUG
	if (pmIrixDebug & DBG_IRIX_XBOW)
	    __pmNotifyErr(LOG_DEBUG,
			 "xbow_store: store %d for xbow %s(%d) (was %d)\n",
			 av.l, xbows[x].extname, x, xbows[x].mapped);
#endif

	if (av.l == 0 && xbows[x].mapped != 0)
	    xbow_stop(x);
	else if (av.l != 0 && xbows[x].mapped == 0)
	    xbow_start(x);
    }
    return 0;
}

void
xbow_fetch_setup(void)
{
    int	x;
    int	p;

    for (x = 0; x < n_xbows; x++)
    	xbows[x].fetched = 0;
    for (p = 0; p < n_xbowportln; p++)
	xbowports[p].fetched = 0;
    return;
}

static int
fetch_xbow(pmID pmid, __pmProfile *profp, pmVPCB *vpcb, int m)
{
    int 		x;
    int			p;
    int			sts = 0;
    int			nval;
    pmAtomValue 	av;
    _pm_xbow		*xb_ptr = NULL;
    _pm_xbowport	*xbp_ptr = NULL;

    for (x = 0; x < n_xbows; x++) {
	xb_ptr = &(xbows[x]);
	if (__pmInProfile(indomtab[PM_INDOM_XBOW].i_indom, profp, xb_ptr->id)) {
	    vpcb->p_nval++;
	    xb_ptr->fetched = 1;
	}
    }

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_XBOW)
	__pmNotifyErr(LOG_DEBUG,
		     "fetch_xbow: num of inst = %d\n",
                     vpcb->p_nval);
#endif

    sizepool(vpcb);
    nval = 0;

    for (x = 0; x < n_xbows; x++) {
	xb_ptr = &(xbows[x]);
	if (!xb_ptr->fetched)
	    continue;

	switch(pmid) {
	case PMID(1,42,2):		/* hinv.map.xbow */
	    av.cp = xb_ptr->hwgname;
	    break;
	case PMID(1,42,5):		/* irix.xbow.switch */
	    av.ul = xb_ptr->mapped;
	    break;
	case PMID(1,42,6):		/* irix.xbow.nports */
	    av.ul = xb_ptr->nports;
	    break;
	case PMID(1,42,7):		/* irix.xbow.gen */
	    av.ul = xb_ptr->gen;
	    break;
	case PMID(1,42,8):		/* irix.xbow.total.src */
	    for (p = 0, av.ull = 0; p < XB_MON_LINK_MASK+1; p++) {
		xbp_ptr = XBOWPORT_HNDL(x,p);
		if (XBOWPORT_ACTIVE(xbp_ptr))
		    av.ull += xbp_ptr->cntrs->vsrc;
	    }
	    break;
	case PMID(1,42,9):		/* irix.xbow.total.dst */
	    for (p = 0, av.ull = 0; p < XB_MON_LINK_MASK+1; p++) {
		xbp_ptr = XBOWPORT_HNDL(x,p);
		if (XBOWPORT_ACTIVE(xbp_ptr))
		    av.ull += xbp_ptr->cntrs->vdst;
	    }
	    break;
	case PMID(1,42,10):		/* irix.xbow.total.rrcv */
	    for (p = 0, av.ull = 0; p < XB_MON_LINK_MASK+1; p++) {
		xbp_ptr = XBOWPORT_HNDL(x,p);
		if (XBOWPORT_ACTIVE(xbp_ptr))
		    av.ull += xbp_ptr->cntrs->crcv;
	    }
	    break;
	case PMID(1,42,11):		/* irix.xbow.total.rxmt */
	    for (p = 0, av.ull = 0; p < XB_MON_LINK_MASK+1; p++) {
		xbp_ptr = XBOWPORT_HNDL(x,p);
		if (XBOWPORT_ACTIVE(xbp_ptr))
		    av.ull += xbp_ptr->cntrs->cxmt;
	    }
	    break;
	default:
	    return PM_ERR_PMID;
	}

	if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], 
			      meta[m].m_desc.type)) < 0)
	    return sts;
	vpcb->p_vp[nval].inst = xb_ptr->id;
	if (nval == 0)
	    vpcb->p_valfmt = sts;
	nval++;
    }
    return sts;
}

/*ARGSUSED*/
static int
fetch_xbowport(pmID pmid, __pmProfile *profp, pmVPCB *vpcb, int m)
{
    int 		p;
    int			sts = 0;
    int			nval;
    pmAtomValue         av;
    void	        *vp;
    _pm_xbowport	*xbp_ptr = NULL;

    for (p = 0; p < n_xbowportln; p++) {
	xbp_ptr = &(xbowports[p]);
	if (xbp_ptr->cntrs != NULL &&
	    __pmInProfile(indomtab[PM_INDOM_XBOWPORT].i_indom, 
			 profp, xbp_ptr->id) &&
	    XBOWPORT_ACTIVE(xbp_ptr)) {

	    vpcb->p_nval++;
	    xbp_ptr->fetched = 1;
	}
    }

    sizepool(vpcb);
    nval = 0;

    for (p = 0; p < n_xbowportln; p++) {
	xbp_ptr = &(xbowports[p]);
	if (!xbp_ptr->fetched)
	    continue;

	vp = (void *)&((char *)(xbp_ptr->cntrs))[(ptrdiff_t)meta[m].m_offset];
	avcpy(&av, vp, meta[m].m_desc.type);
	
	if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], 
			      meta[m].m_desc.type)) < 0)
	    return sts;
	vpcb->p_vp[nval].inst = xbp_ptr->id;
	if (nval == 0)
	    vpcb->p_valfmt = sts;
	nval++;
    }
    return sts;
}

int
xbow_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int                 i;
    int                 sts = 0;
    pmAtomValue         av;

    if (direct_map) {
        __pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
        if (i < nmeta && pmid == meta[i].m_desc.pmid)
            goto doit;

        __pmNotifyErr(LOG_WARNING, 
                     "xbow_fetch: direct mapping failed for %s (!= %s)\n",
                     pmIDStr(pmid), pmIDStr(meta[i].m_desc.pmid));
        direct_map = 0;
    }

    for (i = 0; i < nmeta; i++)
        if (pmid == meta[i].m_desc.pmid)
            break;

    if (i >= nmeta) {
        vpcb->p_nval = 0;
        return PM_ERR_PMID;
    }

 doit:
    if (meta[i].m_desc.type == PM_TYPE_NOSUPPORT) {
	vpcb->p_nval = PM_ERR_APPVERSION;
	return 0;
    }

    if (meta[i].m_desc.indom == indomtab[PM_INDOM_XBOW].i_indom)
    	sts = fetch_xbow(pmid, profp, vpcb, i);
    else if (meta[i].m_desc.indom == indomtab[PM_INDOM_XBOWPORT].i_indom)
    	sts = fetch_xbowport(pmid, profp, vpcb, i);
    else {
	if (pmid == PMID(1,42,1))	/* hinv.nxbow */
	    av.ul = n_xbows;
	else if (pmid == PMID(1,42,3))	/* irix.xbow.active.xbows */
	    av.ul = n_mappedxbows;
	else if (pmid == PMID(1,42,4))	/* irix.xbow.active.ports */
	    av.ul = n_xbowports;
	else {
	    return PM_ERR_PMID;
	}

	vpcb->p_nval = 1;
	vpcb->p_vp[0].inst = PM_IN_NULL;
	sts = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].m_desc.type);
	vpcb->p_valfmt = sts;
    }

    return sts;
}

#endif /* 6.4 or later */
