#ident "$Id: xlv.c,v 1.4 1998/06/15 05:51:55 tes Exp $"

/*
 * Handle metrics for cluster xlv (45)
 *
 */

#if BEFORE_IRIX6_4

#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

int                     xlv_instLen = 0;    /* number xlv volumes */

/*ARGSUSED*/
void
xlv_init(int reset)
{
}

void
xlv_fetch_setup(void)
{
}

static pmMeta   meta[] = {
/* hinv.nxlv_volumes */
 { 0, { PMID(1,45,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* other */
 { 0, { 0, PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }
};

int
xlv_desc(pmID pmid, pmDesc *desc)
{
    if (pmid == PMID(1,45,1)) {	/* hinv.nxlv_volumes is always supported */
        *desc = meta[0].m_desc;
        return 0;
    }

    meta[1].m_desc.pmid = pmid;
    *desc = meta[1].m_desc;
    return 0;
}

/*ARGSUSED*/
int
xlv_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int         sts = 0;
    pmAtomValue av;

    if (pmid == PMID(1,45,1)) {	/* hinv.nxlv_+volumes is always supported */
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

#else

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./xlv.h"

static xlv_stat_t	xlv_stat;

static pmMeta		meta[] = {
/* hinv.nxlv_volumes */
  { NULL, { PMID(1,45,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.read */
  { (char *)&xlv_stat.xs_ops_read, { PMID(1,45,2), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.write */
  { (char *)&xlv_stat.xs_ops_write, { PMID(1,45,3), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.read_bytes */
  { (char *)&xlv_stat.xs_io_rblocks, { PMID(1,45,4), PM_TYPE_U64, PM_INDOM_XLV, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.xlv.write_bytes */
  { (char *)&xlv_stat.xs_io_wblocks, { PMID(1,45,5), PM_TYPE_U64, PM_INDOM_XLV, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.xlv.stripe_ops */
  { (char *)&xlv_stat.xs_stripe_io, { PMID(1,45,6), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.stripe_units */
  { (char *)&xlv_stat.xs_su_total, { PMID(1,45,7), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.aligned.full_width */
  { (char *)&xlv_stat.xs_align_full_sw, { PMID(1,45,8), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.aligned.lt_width */
  { (char *)&xlv_stat.xs_align_less_sw, { PMID(1,45,9), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.aligned.gt_width */
  { (char *)&xlv_stat.xs_align_more_sw, { PMID(1,45,10), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.aligned.part_unit */
  { (char *)&xlv_stat.xs_align_part_su, { PMID(1,45,11), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.unaligned.full_width */
  { (char *)&xlv_stat.xs_unalign_full_sw, { PMID(1,45,12), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.unaligned.lt_width */
  { (char *)&xlv_stat.xs_unalign_less_sw, { PMID(1,45,13), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.unaligned.gt_width */
  { (char *)&xlv_stat.xs_unalign_more_sw, { PMID(1,45,14), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.unaligned.part_unit */
  { (char *)&xlv_stat.xs_unalign_part_su, { PMID(1,45,15), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.largest_io.stripes */
  { (char *)&xlv_stat.xs_max_su_cnt, { PMID(1,45,16), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xlv.largest_io.count */
  { (char *)&xlv_stat.xs_max_su_cnt, { PMID(1,45,17), PM_TYPE_U32, PM_INDOM_XLV, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

};

static xlv_attr_cursor_t _cursor;
static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched = 0;
static int		direct_map = 1;
static int		instSize = 0;

pm_xlv_inst		*xlv_inst = NULL;
int			xlv_instLen = 0;	/* Number of instances */
int			xlv_numLocks = 0;	/* Potential number of subvol */
int			xlv_initDone = 0;	/* Initialised? */

static int
free_subvol_space(xlv_tab_subvol_t *subvol)
{
    if (subvol == NULL)
	return 0;

    free(subvol);
    subvol = NULL;
    return (0);
}

static xlv_tab_subvol_t *
get_subvol_space( void )
{
    xlv_tab_subvol_t        *subvol = NULL;

    subvol = malloc(sizeof(xlv_tab_subvol_t));

    if (subvol == NULL) {
	__pmNotifyErr(LOG_ERR, 
		      "get_subvol_space: Failed to allocate %d bytes for subvol",
		      sizeof(xlv_tab_subvol_t));
	return NULL;
    }
 
    bzero(subvol, sizeof(xlv_tab_subvol_t));
    return subvol;
}

static int
free_vol_space(xlv_tab_vol_entry_t *vol)
{
#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_XLV)
	__pmNotifyErr(LOG_DEBUG, "free_vol_space: Entered for 0x%p\n", vol);
#endif

    if (vol == NULL)
        return 0;

    free_subvol_space(vol->log_subvol);
    free_subvol_space(vol->data_subvol);
    free_subvol_space(vol->rt_subvol);

    if (vol->nodename) {
        free(vol->nodename);
	vol->nodename = NULL;
    }
    free(vol);
    vol = NULL;

    return 0;
}

static void
bzero_vol_space(xlv_tab_vol_entry_t *vol)
{
    xlv_tab_subvol_t	*log = vol->log_subvol;
    xlv_tab_subvol_t	*data = vol->data_subvol;
    xlv_tab_subvol_t	*rt = vol->rt_subvol;

    bzero(vol, sizeof(xlv_tab_vol_entry_t));

    vol->log_subvol = log;
    vol->data_subvol = data;
    vol->rt_subvol = rt;

    bzero(vol->log_subvol, sizeof(xlv_tab_subvol_t));
    bzero(vol->data_subvol, sizeof(xlv_tab_subvol_t));
    bzero(vol->rt_subvol, sizeof(xlv_tab_subvol_t));
}

static xlv_tab_vol_entry_t *
get_vol_space(void)
{
    xlv_tab_vol_entry_t *vol;

    vol = malloc(sizeof(xlv_tab_vol_entry_t));

    if (vol == NULL) {
	__pmNotifyErr(LOG_ERR, "get_vol_space: Failed to allocate %d bytes\n",
		      sizeof(xlv_tab_vol_entry_t));
        return NULL;
    }

    bzero(vol, sizeof(xlv_tab_vol_entry_t));

    if ((NULL == (vol->log_subvol = get_subvol_space())) ||
        (NULL == (vol->data_subvol = get_subvol_space())) ||
        (NULL == (vol->rt_subvol = get_subvol_space()))) {
        free_vol_space(vol);
        return NULL;
    }

    return vol;
}

static int
xlv_load_subvols(void)
{
    xlv_tab_subvol_t    *subvol;
    uint_t              uuid_sts;
    int                 sts;
    int                 i;
    xlv_attr_req_t      req;
    
    _cursor.subvol = -1;
    subvol = get_subvol_space();

    for (i = 0; i < xlv_numLocks; i++) {

	if (i > 0)
	    bzero(subvol, sizeof(xlv_tab_subvol_t));

        if (subvol == NULL) {
	    __pmNotifyErr(LOG_ERR,
			  "xlv_load_subvols: Failed to allocate space for subvol\n");
            return -1;
        }
        req.attr = XLV_ATTR_SUBVOL;
        req.ar_svp = subvol;

        sts = syssgi(SGI_XLV_ATTR_GET, &_cursor, &req);
        if (sts == -1) {
            if (oserror() == ENFILE) {
#ifdef PCP_DEBUG
		if (pmIrixDebug & DBG_IRIX_XLV)
		    __pmNotifyErr(LOG_DEBUG, 
				  "xlv_load_subvols: End of list reached at %d\n",
				  i);
#endif
                break;
            }
            if (oserror() == ENOENT) {
#ifdef PCP_DEBUG
		if (pmIrixDebug & DBG_IRIX_XLV)
		    __pmNotifyErr(LOG_DEBUG, 
				  "xlv_load_subvols: Empty subvol at %d\n",
				  i);
#endif
                continue;
            }
            if (oserror() == ESTALE) {
		__pmNotifyErr(LOG_ERR,
			      "xlv_load_subvols: XLV configuration may have changed while doing initial probe: %s\n",
			      strerror(oserror()));
		break;
            }
	    free_subvol_space(subvol);
            break;
        }

        if (uuid_is_nil(&(subvol->uuid), &uuid_sts)) {
#ifdef PCP_DEBUG
	    if (pmIrixDebug & DBG_IRIX_XLV)
		__pmNotifyErr(LOG_DEBUG, 
			      "xlv_load_subvols: Empty subvol at %d (uuid id nil)\n",
			      i);
#endif
            continue;
        }

	if (xlv_instLen >= instSize) {
	    instSize += 8;
            xlv_inst = (pm_xlv_inst *)realloc(xlv_inst, instSize * 
					      sizeof(pm_xlv_inst));
            if (xlv_inst == NULL) {
		__pmNotifyErr(LOG_ERR,
			      "xlv_load_subvols: Unable to realloc %d bytes",
			      instSize * sizeof(xlv_inst));
                instSize = 0;
                xlv_instLen = 0;
		free_subvol_space(subvol);
                return -1;
            }
        }

	xlv_inst[xlv_instLen].subvol = _cursor.subvol;
	xlv_inst[xlv_instLen].uuid = subvol->uuid;
        xlv_inst[xlv_instLen].id = -1;
	xlv_inst[xlv_instLen].extname = NULL;
	xlv_inst[xlv_instLen].fetched = 0;
        xlv_instLen++;
    }

#ifdef PCP_DEBUG
	    if (pmIrixDebug & DBG_IRIX_XLV)
		__pmNotifyErr(LOG_DEBUG, 
			      "xlv_load_subvols: Created %d XLV instances\n",
			      xlv_instLen);
#endif

    free_subvol_space(subvol);
    return 0;
}

static int
xlv_match_subvol(xlv_tab_subvol_t *subvol, const char *name, const char *postfix)
{
    uint_t              uuid_sts = uuid_s_ok;
    int                 i;
    int                 l;
    ushort_t            hash;

    if (subvol != NULL) {
        for (i = 0; i < xlv_instLen; i++) {
            if (xlv_inst[i].id < 0 &&
                uuid_equal(&(subvol->uuid), &(xlv_inst[i].uuid), &uuid_sts) &&
                uuid_sts == uuid_s_ok) {

                l = strlen(name);
                l+= strlen(postfix);
                xlv_inst[i].extname = (char *)malloc(l + 4);
                if (xlv_inst[i].extname == NULL) {
		    __pmNotifyErr(LOG_ERR,
				  "xlv_match_subvol: Failed to allocate %d bytes\n",
				  l + 4);
		    xlv_instLen = 0;
                    return -1;
                }

                hash = uuid_hash(&(subvol->uuid), &uuid_sts);
                if (uuid_sts != uuid_s_ok) {
		    __pmNotifyErr(LOG_WARNING,
				  "xlv_match_subvol: Failed to generate hash number for %d\n",
				  i);
                    xlv_inst[i].id = i;
                }
                else
                    xlv_inst[i].id = (int)hash;

		/* Check that the inst id is unique */
		for (;;) {
		    for (l = 0; l < i; l++)
			if (xlv_inst[i].id == xlv_inst[l].id) {
			    xlv_inst[i].id++;
			    break;
			}
		    if (l == i)
			break;
		}

                sprintf(xlv_inst[i].extname,"%s.%s", name, postfix);

#ifdef PCP_DEBUG
		if (pmIrixDebug & DBG_IRIX_XLV)
		    __pmNotifyErr(LOG_DEBUG, 
				  "xlv_match_subvol: Instance [%d]: id = %d, name = %s\n",
				  i, xlv_inst[i].id, xlv_inst[i].extname);

#endif

                break;
            }
        }
    }

    return 0;
}

static int
xlv_load_vols(void)
{
    xlv_tab_vol_entry_t *vol_entry;
    int                 sts;
    int                 v;
    int                 i;
    xlv_attr_req_t      req;
    
    vol_entry = get_vol_space();
    if (vol_entry == NULL) {
	__pmNotifyErr(LOG_ERR,
		      "xlv_load_vols: Failed to allocate for vol_entry\n");
	xlv_instLen = 0;
	return -1;
    }

    for (v = 0; v < xlv_numLocks; v++) {

	if (v > 0)
	    bzero_vol_space(vol_entry);

        req.attr = XLV_ATTR_VOL;
        req.ar_vp = vol_entry;

        sts = syssgi(SGI_XLV_ATTR_GET, &_cursor, &req);
        if (sts == -1) {
            if (oserror() == ENFILE) {
#ifdef PCP_DEBUG
		if (pmIrixDebug & DBG_IRIX_XLV)
		    __pmNotifyErr(LOG_DEBUG, 
				  "xlv_load_vols: End of list reached at %d\n",
				  v);
#endif
                break;
            }
            if (oserror() == ENOENT) {
#ifdef PCP_DEBUG
		if (pmIrixDebug & DBG_IRIX_XLV)
		    __pmNotifyErr(LOG_DEBUG, 
				  "xlv_load_vols: Volume %d empty\n", v);
#endif
                continue;
            }
            if (oserror() == ESTALE) {
		__pmNotifyErr(LOG_ERR,
			      "xlv_load_vols: XLV configuration may have changed during initial probe: %s\n",
			      strerror(oserror()));
                break;
            }
	    __pmNotifyErr(LOG_ERR, "xlv_load_vols: volume %d: %s\n", 
			  v, strerror(oserror()));
            break;
        }

        sts = xlv_match_subvol(vol_entry->log_subvol, vol_entry->name, "log");

	if (sts == 0)
	    sts = xlv_match_subvol(vol_entry->data_subvol, vol_entry->name, "data");
        
	if (sts == 0)
	    sts = xlv_match_subvol(vol_entry->rt_subvol, vol_entry->name, "rt");

    }

    free_vol_space(vol_entry);

    for (i = 0; i < xlv_instLen; i++) {
        if (xlv_inst[i].id < 0) {
	    __pmNotifyErr(LOG_ERR, 
			  "xlv_load_vols: Failed to find volume name for inst %d\n",
			  i);

	    xlv_inst[i].extname = (char *)malloc(strlen("unknown") + 16);
	    if (xlv_inst[i].extname == NULL) {
		__pmNotifyErr(LOG_ERR,
			      "xlv_load_vols: Failed to allocate %d bytes\n",
			      strlen("unknown") + 16);
		xlv_instLen = 0;
		return -1;
	    }
	    sprintf(xlv_inst[i].extname, "unknown.%d", i);
	}
    }

    return 0;
}

int
xlv_setup(void)
{
    int        		sts;
    int         	maxlocks = 0;
    xlv_attr_req_t      req;

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_XLV)
	__pmNotifyErr(LOG_DEBUG,
		      "xlv_setup: Initialising XLV indom\n");
#endif

    /*
     * Regardless of whether we pass or fail, set the init flag so that
     * we don't do this again and again...
     */

    xlv_initDone = 1;

    sts = syssgi(SGI_XLV_ATTR_CURSOR, &_cursor);
    if (sts == -1) {
#ifdef PCP_DEBUG
	if (pmIrixDebug & DBG_IRIX_XLV)
	    __pmNotifyErr(LOG_ERR,
			  "xlv_setup: Failed to get cursor, XLV may not be supported: %s\n",
			  strerror(oserror()));
#endif
        return -1;
    }

    req.attr = XLV_ATTR_LOCKS;
    sts = syssgi(SGI_XLV_ATTR_GET, &_cursor, &req);
    if (sts == -1) {
	if (getuid()) {
	    __pmNotifyErr(LOG_ERR, 
			  "xlv_setup: Failed to get XLV_ATTR_LOCKS: %s\n",
			  strerror(oserror()));
	}
        return -1;
    }

    xlv_numLocks = req.ar_num_locks;
    maxlocks = req.ar_max_locks;

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_XLV)
	__pmNotifyErr(LOG_DEBUG, 
		      "xlv_setup: Allocated subvol locks: %d out of %d\n",
		      xlv_numLocks, maxlocks);
#endif

    req.attr = XLV_ATTR_FLAGS;
    sts = syssgi(SGI_XLV_ATTR_GET, &_cursor, &req);
    if (sts < 0) {
	int err = oserror();
	if (getuid() == 0) {
	    if (err == EINVAL) {
		__pmNotifyErr(LOG_ERR, 
		    "xlv statistics are not available. "
		    "To enable xlv statistics you must install "
		    "patch 1841 (or a successor)");
	    }
	    else {
		__pmNotifyErr(LOG_ERR,
			  "xlv_setup: Failed to get XLV_ATRR_FLAGS on: %s\n",
			  strerror(oserror()));
	    }
	}
	return -1;
    } else if (!(req.ar_flag1 & XLV_FLAG_STAT)) {
	if (getuid() == 0) {
	    __pmNotifyErr(LOG_WARNING, 
		 "xlv_setup: Detected xlv statistics collection turned off. "
		 "Libirixpmda turning on xlv statistics collection.");
	}
	req.cmd = XLV_ATTR_CMD_SET;
	req.ar_flag1 = XLV_FLAG_STAT;
	req.ar_flag2 = 0;
	sts = syssgi(SGI_XLV_ATTR_SET, &_cursor, &req);
	if (sts == -1) {
	    if (getuid() == 0) {
		__pmNotifyErr(LOG_ERR,
			  "xlv_setup: Failed to set XLV_FLAG_STAT on: %s\n",
			  strerror(oserror()));
	    }
	    return -1;
	}
    }

    sts = xlv_load_subvols();
    if (sts >= 0)
	sts = xlv_load_vols();

#ifdef PCP_DEBUG
    if (sts < 0 && pmIrixDebug & DBG_IRIX_XLV)
	__pmNotifyErr(LOG_WARNING, "xlv_setup: XLV metrics may be incomplete\n");
#endif

    return 0;
}

int
reload_xlv(void)
{
    int i;

    for (i = 0; i < xlv_instLen; i++) {
	if (xlv_inst[i].extname != NULL) {
	    free(xlv_inst[i].extname);
	    xlv_inst[i].extname = NULL;
	}
    }

    xlv_instLen = 0;
    xlv_initDone = 0;
    return 0;
}

void
xlv_init(int reset)
{
    int		i;
    int		indomtag; /* Constant from descr in form */

    if (reset) {
	(void)reload_xlv();
	return;
    }
 
    for (i = 0; i < nmeta; i++) {
        indomtag = meta[i].m_desc.indom;
	if (direct_map && meta[i].m_desc.pmid != PMID(1,45,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "xlv_init_client_init: direct map disabled @ meta[%d]", i);
	}	
        if (indomtag == PM_INDOM_NULL)
            continue;
        if (indomtag < 0 || indomtag >= PM_INDOM_NEXT) {
            __pmNotifyErr(LOG_ERR, "xlv_init: bad instance domain (%d) for metric %s\n",
                         indomtag, pmIDStr(meta[i].m_desc.pmid));
            continue;
        }
        /* Replace tag with it's indom */
        meta[i].m_desc.indom = indomtab[indomtag].i_indom;
    }

    for (i = 0; i < nmeta; i++)
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&xlv_stat));
}

int
xlv_desc(pmID pmid, pmDesc *desc)
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

void
xlv_fetch_setup(void)
{
    int			i;

    if (!xlv_initDone) {
	(void)xlv_setup();
	indomtab[PM_INDOM_XLV].i_numinst = xlv_instLen;
    }

    fetched = 0;
    for (i = 0; i < xlv_instLen; i++)
	xlv_inst[i].fetched = 0;
}

int
xlv_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    static int 		xlv_stats_off = 0; /* true if stats turned off */
    pm_xlv_inst		*xlvp;
    xlv_attr_req_t	req;
    int			i;
    int			m;
    int			sts = 0;
    pmAtomValue		av;
    void		*vp;
    int			retry;
    int			nerr;
    int			nval = 0;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        m = pmidp->item - 1;
	if (m < nmeta && pmid == meta[m].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "xlv_fetch: direct mapping failed for %s (!= %s)\n",
		     pmIDStr(pmid), pmIDStr(meta[m].m_desc.pmid));
	direct_map = 0;
    }

    for (m = 0; m < nmeta; m++)
	if (pmid == meta[m].m_desc.pmid)
	    break;

    if (m >= nmeta) {
	vpcb->p_nval = 0;
	return PM_ERR_PMID;
    }

 doit:

    if (pmid == PMID(1,45,1)) {		/* hinv.nxlv_volumes */
	vpcb->p_nval = 1;
	vpcb->p_vp[0].inst = PM_IN_NULL;
	av.ul = xlv_instLen;
	if ((sts = __pmStuffValue(&av, 0, vpcb->p_vp, meta[m].m_desc.type)) < 0)
	    return sts;
	vpcb->p_valfmt = sts;
    }
    else {

	switch (fetched) {
	case 0:
	    for (retry=0; retry < 2; retry++) {
		vpcb->p_nval = 0;
		nerr = 0;

		for (i = 0; nerr == 0 && i < xlv_instLen; i++) {
		    xlvp = &(xlv_inst[i]);
		    if (xlvp->id < 0 || !__pmInProfile(indomtab[PM_INDOM_XLV].i_indom, profp, xlvp->id)) {
			/* not in profile, or id is invalid */
			xlvp->fetched = 0;
			continue; /* next instance */
		    }

		    /* check that xlv stats are turned on */
		    req.attr = XLV_ATTR_FLAGS;
		    sts = syssgi(SGI_XLV_ATTR_GET, &_cursor, &req);
		    if (sts < 0) {
			sts = -oserror();
			__pmNotifyErr(LOG_ERR,
			  "xlv_fetch: Failed to get XLV_ATRR_FLAGS on: %s\n",
			  strerror(-sts));
			vpcb->p_nval = 0;
			fetched = sts;
			return sts; /* fail */
		    } else if (!(req.ar_flag1 & XLV_FLAG_STAT)) {
			if (xlv_stats_off == 0) {
			    __pmNotifyErr(LOG_ERR, 
			     "xlv_fetch: Detected xlv statistics collection turned off.");
			    xlv_stats_off = 1;
			}
			return PM_ERR_AGAIN; /* no values at this time */
		    } else if (xlv_stats_off == 1) {
			__pmNotifyErr(LOG_WARNING, 
			 "xlv_fetch: Detected xlv statistics collection turned back on.");
			xlv_stats_off = 0;
		    }
		    

		    req.ar_statp = &(xlvp->stat);
		    _cursor.subvol = xlvp->subvol;

		    req.attr = XLV_ATTR_STATS;
		    if (syssgi(SGI_XLV_ATTR_GET, &_cursor, &req) == 0) {
			xlvp->fetched = 1;
			vpcb->p_nval++;
			continue; /* next instance */
		    }

		    /*
		     * Error Handling
		     * syssgi(SGI_XLV_ATTR_GET) can fail as follows ...
		     *      EPERM  - user lack permission.
		     *      EINVAL - cannot copyin user arguments to kernel space.
		     *      ESTALE - configuration changed since last cursor update.
		     *      ENOENT - XLV configuration has not been set/assembled.
		     *      EFAULT - copyout to user space failed.
		     *      ENFILE - reach pass end of XLV table (enumeration is done).
		     */
		    sts = -oserror();
		    nerr++;
		    if (sts == -ESTALE) {
			/*
			 * configuration changed since last cursor update
			 */
			__pmNotifyErr(LOG_INFO, "xlv_fetch: xlv has been reconfigured (retry=%d) : %s\n",
			      retry, strerror(oserror()));
			reload_xlv();
			xlv_fetch_setup();
			break; /* retry */
		    }
		    else {
			/*
			 * all other errors => fetch fails immediately (no retry)
			 */
			vpcb->p_nval = 0;
			fetched = sts;
			return sts; /* fail */
		    }
		} /* next instance */

		if (nerr == 0)
		    break; /* no retry necessary */
	    }

	    if (nerr == 0)
		fetched = 1;
	    else
		fetched = sts;

	    break;

	case 1: /* already successfully fetched (for an earlier pmid) */
	    vpcb->p_nval = 0;
	    for (i=0; i < xlv_instLen; i++) {
		xlvp = &(xlv_inst[i]);
		if (xlvp->id >= 0 && xlvp->fetched)
		    vpcb->p_nval++;
	    }
	    break;

	default: /* already tried to fetch for an earlier pmid, but failed */
	    vpcb->p_nval = 0;
	    return fetched; /* fail */
	    /* NOTREACHED */
	}

	
	sizepool(vpcb);
	nval = 0;

	for (i = 0; i < xlv_instLen; i++) {
	    xlvp = &(xlv_inst[i]);
	    if (xlvp->fetched == 0)
		continue;
	    vp = (void *)&((char *)&xlvp->stat)[(ptrdiff_t)meta[m].m_offset];
	    avcpy(&av, vp, meta[m].m_desc.type);

	    /* Convert from blocks to bytes */
	    if (meta[m].m_desc.units.scaleSpace == PM_SPACE_KBYTE)
		av.ull >>= 1;

	    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[m].m_desc.type)) < 0)
		return sts;
	    vpcb->p_vp[nval].inst = xlvp->id;
	    if (nval == 0)
		vpcb->p_valfmt = sts;
	    nval++;
	}
    }

    /*
     * success
     */
    return 0;
}

#endif /* IRIX6.4 and later */
