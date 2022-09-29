/*
 * stream - export stream metrics
 *
 * Copyright 1999, Silicon Graphics, Inc.
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
 */

#ident "$Id: stream.c,v 1.2 1999/10/14 07:21:40 tes Exp $"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/stream.h>
#include <sys/strstat.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./kmemread.h"

/* do what irix/cmd/bsd/netstat/stream.c does
 * and define indices here
 */
#define STR_MSG_INDEX           0 /* msgb structures */
#define STR_MD_INDEX            1
#define STR_BUF64_INDEX         2
#define STR_BUF256_INDEX        3
#define STR_BUF512_INDEX        4
#define STR_BUF2K_INDEX         5
#define STR_PAGE_INDEX          6
#define STR_MAX_INDEX           7


static struct strstat	strstat;
static __uint32_t	str_curpages;
static __uint32_t	str_minpages;
static __uint32_t	str_maxpages;

static pmMeta	meta[] = {
/* irix.stream.streams.use */
  { (char*)&strstat.stream.use, { PMID(1,46,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.streams.max */
  { (char*)&strstat.stream.max, { PMID(1,46,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.streams.fail */
  { (char*)&strstat.stream.fail, { PMID(1,46,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.queues.use */
  { (char*)&strstat.queue.use, { PMID(1,46,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.queues.max */
  { (char*)&strstat.queue.max, { PMID(1,46,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.queues.fail */
  { (char*)&strstat.queue.fail, { PMID(1,46,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.msg_blks.use */
  { (char*)&strstat.buffer[STR_MSG_INDEX].use, { PMID(1,46,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.msg_blks.max */
  { (char*)&strstat.buffer[STR_MSG_INDEX].max, { PMID(1,46,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.msg_blks.fail */
  { (char*)&strstat.buffer[STR_MSG_INDEX].fail, { PMID(1,46,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.msg_dblks.use */
  { (char*)&strstat.buffer[STR_MD_INDEX].use, { PMID(1,46,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.msg_dblks.max */
  { (char*)&strstat.buffer[STR_MD_INDEX].max, { PMID(1,46,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.msg_dblks.fail */
  { (char*)&strstat.buffer[STR_MD_INDEX].fail, { PMID(1,46,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_64.use */
  { (char*)&strstat.buffer[STR_BUF64_INDEX].use, { PMID(1,46,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_64.max */
  { (char*)&strstat.buffer[STR_BUF64_INDEX].max, { PMID(1,46,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_64.fail */
  { (char*)&strstat.buffer[STR_BUF64_INDEX].fail, { PMID(1,46,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_256.use */
  { (char*)&strstat.buffer[STR_BUF256_INDEX].use, { PMID(1,46,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_256.max */
  { (char*)&strstat.buffer[STR_BUF256_INDEX].max, { PMID(1,46,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_256.fail */
  { (char*)&strstat.buffer[STR_BUF256_INDEX].fail, { PMID(1,46,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_512.use */
  { (char*)&strstat.buffer[STR_BUF512_INDEX].use, { PMID(1,46,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_512.max */
  { (char*)&strstat.buffer[STR_BUF512_INDEX].max, { PMID(1,46,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_512.fail */
  { (char*)&strstat.buffer[STR_BUF512_INDEX].fail, { PMID(1,46,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_2048.use */
  { (char*)&strstat.buffer[STR_BUF2K_INDEX].use, { PMID(1,46,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_2048.max */
  { (char*)&strstat.buffer[STR_BUF2K_INDEX].max, { PMID(1,46,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_2048.fail */
  { (char*)&strstat.buffer[STR_BUF2K_INDEX].fail, { PMID(1,46,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_page.use */
  { (char*)&strstat.buffer[STR_PAGE_INDEX].use, { PMID(1,46,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_page.max */
  { (char*)&strstat.buffer[STR_PAGE_INDEX].max, { PMID(1,46,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.bufsz_page.fail */
  { (char*)&strstat.buffer[STR_PAGE_INDEX].fail, { PMID(1,46,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.link_blks.use */
  { (char*)&strstat.linkblk.use, { PMID(1,46,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.link_blks.max */
  { (char*)&strstat.linkblk.max, { PMID(1,46,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.link_blks.fail */
  { (char*)&strstat.linkblk.fail, { PMID(1,46,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.events.use */
  { (char*)&strstat.strevent.use, { PMID(1,46,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.events.max */
  { (char*)&strstat.strevent.max, { PMID(1,46,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.events.fail */
  { (char*)&strstat.strevent.fail, { PMID(1,46,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.qbinfo.use */
  { (char*)&strstat.qbinfo.use, { PMID(1,46,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.qbinfo.max */
  { (char*)&strstat.qbinfo.max, { PMID(1,46,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.qbinfo.fail */
  { (char*)&strstat.qbinfo.fail, { PMID(1,46,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.cur_pages */
  { (char*)0, { PMID(1,46,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.min_pages */
  { (char*)0, { PMID(1,46,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.stream.max_pages */
  { (char*)0, { PMID(1,46,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,1,0,0,PM_COUNT_ONE} } },
};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	fetched;
static int 	direct_map = 1;

void stream_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char*)((ptrdiff_t)(meta[i].m_offset - (char *)&strstat));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,46,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, "stream_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void stream_fetch_setup(void)
{
    fetched = 0;
}

int stream_desc(pmID pmid, pmDesc *desc)
{
    int		i;

    if (direct_map) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	i = pmidp->item - 1;
	if (i < nmeta)
	    goto doit;
    }
    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
doit:
	    *desc = meta[i].m_desc;	/* struct assignment */
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

/*ARGSUSED*/
int stream_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    pmAtomValue		av;
    void		*vp;
    __psunsigned_t      offset;
    int			sts;

    if (direct_map) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	i = pmidp->item - 1;
	if (i < nmeta)
	    goto doit;
    }
    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
doit:
	    if (meta[i].m_desc.type == PM_TYPE_NOSUPPORT) {
		vpcb->p_nval = PM_ERR_APPVERSION;
		return 0;
	    }
	    if (fetched == 0) {
		if (sysmp(MP_SAGET, MPSA_STREAMSTATS, &strstat, sizeof(strstat)) < 0)
		    return -errno;

		/* --- do kmem read calls --- */
		offset = (__psunsigned_t)kernsymb[KS_STR_CURPAGES].n_value;
		sts = kmemread(offset, &str_curpages, sizeof(str_curpages));
		if (sts != sizeof(str_curpages)) {
		    __pmNotifyErr(LOG_WARNING, 
                          "stream.c: read str_curpages: %s", 
			  pmErrStr(-errno));
		    return -errno;
		}
		offset = (__psunsigned_t)kernsymb[KS_STR_MINPAGES].n_value;
		sts = kmemread(offset, &str_minpages, sizeof(str_minpages));
		if (sts != sizeof(str_minpages)) {
		    __pmNotifyErr(LOG_WARNING, 
                          "stream.c: read str_minpages: %s", 
			  pmErrStr(-errno));
		    return -errno;
		}
		offset = (__psunsigned_t)kernsymb[KS_STR_MAXPAGES].n_value;
		sts = kmemread(offset, &str_maxpages, sizeof(str_maxpages));
		if (sts != sizeof(str_maxpages)) {
		    __pmNotifyErr(LOG_WARNING, 
                          "stream.c: read str_maxpages: %s", 
			  pmErrStr(-errno));
		    return -errno;
		}

		fetched = 1;
	    }

	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    switch (pmid) {
		case PMID(1,46,37):
		    av.ul = str_curpages;
		    break;
		case PMID(1,46,38):
		    av.ul = str_minpages;
		    break;
		case PMID(1,46,39):
		    av.ul = str_maxpages;
		    break;
		default:
		    vp = (void *)&((char *)&strstat)[(ptrdiff_t)meta[i].m_offset];
		    avcpy(&av, vp, meta[i].m_desc.type);
		    break;
	    }

	    return vpcb->p_valfmt = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].m_desc.type);
	}
    }
    return PM_ERR_PMID;
}
