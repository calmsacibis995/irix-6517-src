/*
 * Handle metrics for cluster aio (36)
 */

#ident "$Id: aio.c,v 1.16 1998/02/26 22:05:59 kenmcd Exp $"

#include <sys/types.h>
#include <sys/syssgi.h>
#include <errno.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

#if defined(IRIX6_5)
#include <sys/dbacc.h>
#else

/*
 * from irix/kern/sys/kaio.h
 * should be exported into a header file in $ROOT ... this was done for
 * kudzu, but not the earlier IRIXes
 */
#if (_MIPS_SZLONG == 64)
typedef long kaio_field_t;
#else
typedef long long kaio_field_t;
#endif

typedef struct {
     kaio_field_t       kaio_nobuf;
     kaio_field_t       kaio_proc_inprogress;
     kaio_field_t       kaio_proc_maxinuse;
     kaio_field_t       kaio_aio_inuse;
     kaio_field_t       kaio_reads;
     kaio_field_t       kaio_writes;
     kaio_field_t       kaio_read_bytes;
     kaio_field_t       kaio_write_bytes;
     kaio_field_t       kaio_io_errs;
} dba_stat_t;

#endif /* !IRIX6_5 */

static dba_stat_t	kaio_statbuf;

static pmMeta		meta[] = {
/* irix.kaio.nobuf */
  { (char *)&kaio_statbuf.kaio_nobuf, { PMID(1,36,1), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kaio.inuse */
  { (char *)&kaio_statbuf.kaio_aio_inuse, { PMID(1,36,2), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kaio.reads */
  { (char *)&kaio_statbuf.kaio_reads, { PMID(1,36,3), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kaio.writes */
  { (char *)&kaio_statbuf.kaio_writes, { PMID(1,36,4), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kaio.read_bytes */
  { (char *)&kaio_statbuf.kaio_read_bytes, { PMID(1,36,5), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.kaio.write_bytes */
  { (char *)&kaio_statbuf.kaio_write_bytes, { PMID(1,36,6), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.kaio.errors */
  { (char *)&kaio_statbuf.kaio_io_errs, { PMID(1,36,7), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)

/* irix.kaio.free */
  { 0, { PMID(1,36,8), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.kaio.proc_maxinuse */
  { 0, { PMID(1,36,9), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.kaio.inprogress */
  { 0, { PMID(1,36,10), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },

#else

/* irix.kaio.free */
  { (char *)&kaio_statbuf.kaio_free, { PMID(1,36,8), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kaio.proc_maxinuse */
  { (char *)&kaio_statbuf.kaio_proc_maxinuse, { PMID(1,36,9), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kaio.inprogress */
  { (char *)&kaio_statbuf.kaio_inprogress, { PMID(1,36,10), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },

#endif /* IRIX6_5 or later */
};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	direct_map = 1;
static int	fetched;

void
irixpmda_aio_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&kaio_statbuf));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,36,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "aio_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void
irixpmda_aio_fetch_setup(void)
{
    fetched = 0;
}

int
irixpmda_aio_desc(pmID pmid, pmDesc *desc)
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

/*ARGSUSED*/
int
irixpmda_aio_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		sts;
    __int64_t	*ip;
    static int	not_installed = 0;
    pmAtomValue	atom;

    if (not_installed) {
	vpcb->p_nval = -ENOPKG;
	return 0;
    }

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "aio_fetch: direct mapping failed for %s (!= %s)\n",
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

    if (fetched == 0) {
#if defined(IRIX6_5)
	/*
	 * the "-1" requests the metrics be summed across all of
	 * the per CPU structure instances
	 */
	sts = (int)syssgi(SGI_DBA_GETSTATS, &kaio_statbuf, sizeof(kaio_statbuf), -1);
#else
	sts = (int)syssgi(SGI_KAIO_STATS, &kaio_statbuf, sizeof(kaio_statbuf));
#endif
	if (sts != 0) {
	    if (not_installed == 0 || (errno != ENOPKG && errno != ENOENT)) {
		if (errno == ENOPKG || errno == ENOENT)
		    __pmNotifyErr(LOG_WARNING, 
				  "Kernel AIO option not installed");
		else
		    __pmNotifyErr(LOG_WARNING, "aio.c: syssgi: %s", pmErrStr(-errno));
		if (not_installed == 0 && (errno == ENOPKG || errno == ENOENT))
		    /* once is enough in the log! */
		    not_installed = 1;
	    }
	    vpcb->p_nval = -ENOPKG;
	    return 0;
	}
	fetched = 1;
    }
    if (fetched) {
	vpcb->p_nval = 1;
	vpcb->p_vp[0].inst = PM_IN_NULL;
	ip = (__int64_t *)&((char *)&kaio_statbuf)[(ptrdiff_t)meta[i].m_offset];
	atom.ll = *ip;
	if ((sts = __pmStuffValue(&atom, 0, vpcb->p_vp, meta[i].m_desc.type)) < 0)
	    return sts;
	vpcb->p_valfmt = sts;
    }
    else
	vpcb->p_nval = 0;
    return 0;
}
