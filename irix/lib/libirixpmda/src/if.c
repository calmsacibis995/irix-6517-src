/*
 * Handle metrics for cluster if (25)
 *
 * Code built by newcluster on Thu Jun  9 15:23:36 EST 1994
 */

#ident "$Id: if.c,v 1.30 1999/10/14 07:21:40 tes Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include <net/if.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./kmemread.h"
#include "./xpc.h"

/*
 * Table of ifnet entries, one entry per interface.
 * (this is exported to indom.c)
 */
struct ifnet	*_ifnet_table = (struct ifnet *)0;
int		 _ifnet_n = 0; /* current number of entries */

static int	 _ifnet_max = 0; /* maximum number of entries (before realloc needed) */
static struct ifnet ifnet; /* used for offset calculation */

static pmMeta		meta[] = {
/*
 * Generic interface statistics
 */
/* irix.network.interface.collisions */
  { (char *)&ifnet.if_data.ifi_collisions, { PMID(1,25,1), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.mtu */
  { (char *)&ifnet.if_data.ifi_mtu, { PMID(1,25,2), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_INSTANT, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.interface.noproto */
  { (char *)&ifnet.if_data.ifi_noproto, { PMID(1,25,3), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.baudrate */
#if BEFORE_IRIX6_5
  { (char *)&ifnet.if_data.ifi_baudrate, { PMID(1,25,4), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_DISCRETE, {1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0} } },
#else
  { (char *)0, { PMID(1,25,4), PM_TYPE_U64, PM_INDOM_NETIF, PM_SEM_DISCRETE, {1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0} } },
#endif
/*
 * Input metrics
 */
/* irix.network.interface.in.errors */
  { (char *)&ifnet.if_data.ifi_ierrors, { PMID(1,25,5), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.in.packets */
  { (char *)&ifnet.if_data.ifi_ipackets, { PMID(1,25,6), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.in.bytes */
  { (char *)&ifnet.if_data.ifi_ibytes, { PMID(1,25,7), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.interface.in.mcasts */
  { (char *)&ifnet.if_data.ifi_imcasts, { PMID(1,25,8), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.in.drops */
  { (char *)&ifnet.if_data.ifi_iqdrops, { PMID(1,25,9), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/*
 * Output metrics
 */
/* irix.network.interface.out.errors */
  { (char *)&ifnet.if_data.ifi_oerrors, { PMID(1,25,10), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.out.packets */
  { (char *)&ifnet.if_data.ifi_opackets, { PMID(1,25,11), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.out.bytes */
  { (char *)&ifnet.if_data.ifi_obytes, { PMID(1,25,12), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.interface.out.mcasts */
  { (char *)&ifnet.if_data.ifi_omcasts, { PMID(1,25,13), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.out.drops */
  { (char *)&ifnet.if_data.ifi_odrops, { PMID(1,25,14), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.out.qdrops */
  { (char *)&ifnet.if_snd.ifq_drops, { PMID(1,25,15), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.out.qlength */
  { (char *)&ifnet.if_snd.ifq_len, { PMID(1,25,16), PM_TYPE_32, PM_INDOM_NETIF, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.out.qmax */
  { (char *)&ifnet.if_snd.ifq_maxlen, { PMID(1,25,17), PM_TYPE_32, PM_INDOM_NETIF, PM_SEM_INSTANT, {0,0,0,0,0,PM_COUNT_ONE} } },
/*
 * derived metrics
 */
/* irix.network.interface.total.errors */
  { (char *)0, { PMID(1,25,18), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.total.packets */
  { (char *)0, { PMID(1,25,19), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.total.bytes */
  { (char *)0, { PMID(1,25,20), PM_TYPE_U64, PM_INDOM_NETIF, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.interface.total.mcasts */
  { (char *)0, { PMID(1,25,21), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.interface.total.drops */
  { (char *)0, { PMID(1,25,22), PM_TYPE_U32, PM_INDOM_NETIF, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/*
 * xpc metrics
 */
/* irix.xpc.network.interface.in.bytes */
  { (char *)&ifnet.if_data.ifi_ibytes, { PMID(1,25,23), PM_TYPE_U64, PM_INDOM_NETIF, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.xpc.network.interface.out.bytes */
  { (char *)&ifnet.if_data.ifi_obytes, { PMID(1,25,24), PM_TYPE_U64, PM_INDOM_NETIF, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.xpc.network.interface.total.bytes */
  { (char *)0, { PMID(1,25,25), PM_TYPE_U64, PM_INDOM_NETIF, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },

};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched;
static int		*interesting = (int *)0;
static int		direct_map = 1;

int
reload_ifnet(void)
{
    int			sts;
    int			i;
    __psunsigned_t		offset;
    __psunsigned_t		name_offset;
    struct ifnet 	*ifp;
    char		name_buf[IFNAMSIZ];
    static int		first_time=1;

    if (first_time) {
	_ifnet_n = 0;
	_ifnet_max = 4;
	_ifnet_table = (struct ifnet *)malloc(_ifnet_max * sizeof(struct ifnet));
	interesting = (int *)malloc(_ifnet_max * sizeof(int));
	if (_ifnet_table == (struct ifnet *)0 || interesting == (int *)0) {
	    __pmNotifyErr(LOG_WARNING, "if.c: malloc: %s", pmErrStr(-errno));
	    indomtab[PM_INDOM_NETIF].i_numinst = 0;
	    return -errno;
	}

	first_time = 0;
    }
    else {
	/*
	 * free all the if_name buffers
	 * before we overwrite them
	 */
	for (i=0; i < _ifnet_n; i++)
	    free(_ifnet_table[i].if_name);
    }

    /*
     * Get the address of the first ifnet entry
     * (do this every time in case it moves)
     */
    offset = (__psunsigned_t)kernsymb[KS_IFNET].n_value;
    sts = kmemread(offset, &ifp, sizeof(struct ifnet *));
    if (sts != sizeof(struct ifnet *)) {
	/* can't resolve double indirect */
	if (sts == -EFAULT)
	    __pmNotifyErr(LOG_WARNING, "irix.network.interface metrics unavailble: kmem kernel interface unavailable\n");
	else
	    __pmNotifyErr(LOG_WARNING, "if.c: read &ifnet pointer: %s", pmErrStr(-errno));
	indomtab[PM_INDOM_NETIF].i_numinst = 0;
	return sts;
    }
    offset = (__psunsigned_t)ifp;
    _ifnet_n=0;

    for (;;) {
	if (_ifnet_n >= _ifnet_max) {
	    _ifnet_max += 4;
	    _ifnet_table = (struct ifnet *)realloc((void *)_ifnet_table,
		_ifnet_max * sizeof(struct ifnet));
	    interesting = (int *)realloc((void *)interesting,
		_ifnet_max * sizeof(int));
	    if (_ifnet_table == (struct ifnet *)0 || interesting == (int *)0) {
		__pmNotifyErr(LOG_WARNING, "if.c: realloc: %s", pmErrStr(-errno));
		indomtab[PM_INDOM_NETIF].i_numinst = 0;
		return -errno;
	    }
	}

	if (VALID_KMEM_ADDR(offset) == 0) {
	    /*
	     * Probably a data structure mismatch between
	     * the running kernel and <net/if.h>.
	     * This is a known problem when running 5.2MR.
	     * We refuse to collect any metrics for this cluster.
	     */
	    _ifnet_n = 0;
	    return 0;
	}

#ifdef PCP_DEBUG
	if (pmIrixDebug & DBG_IRIX_IF)
	    __pmNotifyErr(LOG_DEBUG,
	    		  "reload_ifnet: reading ifnet struct for %d at %p\n",
			  _ifnet_n, offset);
#endif

	ifp = &_ifnet_table[_ifnet_n];
	sts = kmemread(offset, (void *)ifp, sizeof(struct ifnet));
	if (sts != sizeof(struct ifnet)) {
	    /* 
	     * Assume this means end of the list
	     * (do not include this entry)
	     */
	    break;
	}

	/* and the interface name string */
	name_offset = (__psunsigned_t)ifp->if_name;

#ifdef PCP_DEBUG
	if (pmIrixDebug & DBG_IRIX_IF)
	    __pmNotifyErr(LOG_DEBUG,
	    		  "reload_ifnet: reading name for %d at %p\n",
			  _ifnet_n, offset);
#endif

	sts = unaligned_kmemread(name_offset, (void *)name_buf, sizeof(name_buf));
	if (sts > 0)
	    ifp->if_name = strdup(name_buf);
	else
	    ifp->if_name = strdup("unknown");

	_ifnet_n++;

	if (ifp->if_next == (struct ifnet *)0) {
	    /* end of the list */
	    break;
	}

	/* march forward */
	offset = (__psunsigned_t)ifp->if_next;
    }

    return _ifnet_n;
}


void
if_init(int reset)
{
    int		i;
    int         indomtag; /* Constant from descr in form */

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&ifnet));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,25,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "if_init: direct map disabled @ meta[%d]", i);
	}
    }

    for (i = 0; i < nmeta; i++) {
        indomtag = meta[i].m_desc.indom;
        if (indomtag == PM_INDOM_NULL)
            continue;
        if (indomtag < 0 || indomtag >= PM_INDOM_NEXT) {
            __pmNotifyErr(LOG_ERR, "if_init: bad instance domain (%d) for metric %s\n",
                         indomtag, pmIDStr(meta[i].m_desc.pmid));
            continue;
        }
        /* Replace tag with it's indom */
        meta[i].m_desc.indom = indomtab[indomtag].i_indom;
    }

    /* TODO: Determine why this is done here
    indomtab[PM_INDOM_NETIF].i_numinst = reload_ifnet();
    */

}

void if_fetch_setup(void)
{
    fetched = 0;
}

int if_desc(pmID pmid, pmDesc *desc)
{
    int		i;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    *desc = meta[i].m_desc;	/* struct assignment */
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

int if_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    int			j;
    int			nval;
    int			sts;
    __uint64_t		*cp;
    __uint64_t		*cp2;
    pmAtomValue		av;
    pmAtomValue		*avp;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "if_fetch: direct mapping failed for %s (!= %s)\n",
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

    if (fetched == 0) {
	reload_ifnet();
	fetched = 1;
    }
    
    for (j=0; j < _ifnet_n; j++) {
	interesting[j] = __pmInProfile(meta[i].m_desc.indom, profp, _ifnet_table[j].if_index);
	if (interesting[j])
	    vpcb->p_nval++;
    }
    
    sizepool(vpcb);
    nval = 0;
    
    for (j=0; j < _ifnet_n; j++) {
	register struct ifnet *ifp = &_ifnet_table[j];

	if (interesting[j] == 0)
	    continue;

	avp = &av;

	switch (pmid) {
	    /* derived metrics */
#if !BEFORE_IRIX6_5
	case PMID(1,25,4): /* irix.network.interface.baudrate */
	    av.ull = if_getbaud(ifp->if_data.ifi_baudrate);
	    break;
#endif
	case PMID(1,25,18):		/* total.errors */
	    av.ul = ifp->if_data.ifi_ierrors + ifp->if_data.ifi_oerrors;
	    break;
	case PMID(1,25,19):		/* total.packets */
	    av.ul = ifp->if_data.ifi_ipackets + ifp->if_data.ifi_opackets;
	    break;
	case PMID(1,25,20):		/* total.bytes */
	case PMID(1,25,25):		/* xpc total.bytes */
	    /* NOTE: [TS: 2/Feb/99]
	     * We have changed cp & cp2 to use the ibytes pmid
	     * and the obytes pmid.
	     * Doing this will mean that the xpc ibytes and xpc obytes
	     * will add up to the total.
	     */
	    av.ul = ifp->if_data.ifi_ibytes;
	    cp = XPCincr(PMID(1,25,7), j, av.ul);
	    av.ul = ifp->if_data.ifi_obytes;
	    cp2 = XPCincr(PMID(1,25,12), j, av.ul);
	    if (meta[i].m_desc.type == PM_TYPE_U32)
		av.ul = (__uint32_t)(*cp + *cp2);
	    else
		av.ull = *cp + *cp2;
	    break;
	case PMID(1,25,21):		/* total.mcasts */
	    av.ul = ifp->if_data.ifi_imcasts + ifp->if_data.ifi_omcasts;
	    break;
	case PMID(1,25,22):		/* total.drops */
	    av.ul = ifp->if_snd.ifq_drops + ifp->if_data.ifi_odrops;
	    break;
	default:			/* all others use the offset mechanism */
	    avp = (pmAtomValue *)&((char *)ifp)[(ptrdiff_t)meta[i].m_offset];
	    if (meta[i].m_desc.units.scaleSpace == PM_SPACE_BYTE &&
		meta[i].m_desc.sem == PM_SEM_COUNTER) {
		cp = XPCincr(pmid, j, avp->ul);
		if (meta[i].m_desc.type == PM_TYPE_U32)
		    av.ul = (__uint32_t)(*cp);
		else
		    av.ull = *cp;
		avp = &av;
	    }
	    break;
	}

	if ((sts = __pmStuffValue(avp, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
	    return sts;

	if (nval == 0)
	    vpcb->p_valfmt = sts;

	vpcb->p_vp[nval++].inst = ifp->if_index;

    }
    return 0;
}
