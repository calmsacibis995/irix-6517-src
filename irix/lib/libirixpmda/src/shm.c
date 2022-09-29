/*
 * Handle metrics for ipc/shm (29)
 *
 * Note: from IRIX 6.2 on, a struct shmid_ds has two different
 *	 definitions, one within the kernel and one above the
 *	 shmctl() interface ... this source file uses shmctl()
 *	 to access the latter.  See also shm_kmem.c.
 */

#ident "$Id: shm.c,v 1.22 1997/11/21 06:34:28 kenmcd Exp $"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./shm.h"
#include "./cluster.h"

static pmMeta		meta[] = {
/* irix.ipc.shm.segsz */
#if (_MIPS_SZLONG == 32)
  { 0, { PMID(1,29,1), PM_TYPE_32, PM_INDOM_SHM, PM_SEM_INSTANT, {1,0,0,PM_SPACE_BYTE,0,0} } },
#elif (_MIPS_SZLONG == 64)
  { 0, { PMID(1,29,1), PM_TYPE_U64, PM_INDOM_SHM, PM_SEM_INSTANT, {1,0,0,PM_SPACE_BYTE,0,0} } },
#else
  bozo
#endif
/* irix.ipc.shm.nattch */
  { 0, { PMID(1,29,2), PM_TYPE_U32, PM_INDOM_SHM, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } }
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched = 0;
static int		shm_inst_n;
static int		shm_inst_len = 0;
static struct shmid_ds	*shm_inst = (struct shmid_ds *)0;
static int		*shmid_list = (int *)0;

/* imported from shm_kmem.c */
extern shm_map_t	*shm_map;

void shm_init(int reset)
{
    int	i;
    int	indomtag;		/* Constant from descr in form */

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	indomtag = meta[i].m_desc.indom;
	if (indomtag == PM_INDOM_NULL)
	    continue;
	if (indomtag < 0 || indomtag >= PM_INDOM_NEXT) {
	    __pmNotifyErr(LOG_ERR, "shm_init: bad instance domain (%d) for metric %s\n",
			 indomtag, pmIDStr(meta[i].m_desc.pmid));
	    continue;
	}
	/* Replace tag with its indom */
	meta[i].m_desc.indom = indomtab[indomtag].i_indom;
    }

    return;
}

void shm_fetch_setup(void)
{
    extern int reload_shm(void);

    indomtab[PM_INDOM_SHM].i_numinst = reload_shm();

    fetched = 0;
    shm_inst_n = 0;
}

int shm_desc(pmID pmid, pmDesc *desc)
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

int shm_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    int			j;
    int			k;
    int			e;
    int			id;
    int			sts;
    int			nval;
    int			profile_found=0;
    pmAtomValue		av;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    if (fetched == 0) {
		/*
		 * Extract the instance list (i.e. IPC/SHM IDs) from the
		 * supplied profile.
		 * The profile MUST be an explicit inclusion list
		 * (i.e. exclude all, include some)
		 */

		for (j=0; j < profp->profile_len; j++) {

		    if (profp->profile[j].indom != meta[i].m_desc.indom) {
			continue;
		    }

		    if (profp->profile[j].state != PM_PROFILE_EXCLUDE ||
			profp->profile[j].instances_len == 0)
			break;

		    profile_found++;
		    for (k=0; k < profp->profile[j].instances_len; k++) {
			/*
			 * Extract the requested shmid_ds instances.
			 * We simply ignore non-existent instances.
			 */
			if (shm_inst_n >= shm_inst_len) {
			    /* get some more space! */
			    shm_inst_len += 8;
			    if ((shm_inst = (struct shmid_ds *)realloc(shm_inst,
				shm_inst_len * sizeof(struct shmid_ds))) == (struct shmid_ds *)NULL) {
				e = -errno;
				__pmNotifyErr(LOG_ERR, "shm_fetch: failed to realloc %d shmid_ds entries : %s\n", shm_inst_len, pmErrStr(-errno));
				return e;
			    }

			    if ((shmid_list = (int *)realloc(shmid_list,
				shm_inst_len * sizeof(int))) == (int *)NULL) {
				e = -errno;
				__pmNotifyErr(LOG_ERR, "shm_fetch: failed to realloc %d ints for shmid list : %s\n", shm_inst_len, pmErrStr(-errno));
				return e;
			    }
			}
			id = profp->profile[j].instances[k];

			if ((sts = shmctl(id, IPC_STAT, &shm_inst[shm_inst_n])) < 0) {
			    __pmNotifyErr(LOG_WARNING, "shm_fetch: ignored non-existing shm instance pmid=%d shmctl(id=%d) : %s\n", pmid, id, pmErrStr(-errno));
			    continue;
			}
			shmid_list[shm_inst_n++] = id;
		    }
		}

		if (profile_found == 0) {
		    /* this situation would not be caught above */
		    if (indomtab[PM_INDOM_SHM].i_numinst == 0)
			vpcb->p_nval = PM_ERR_VALUE;
		    else
			vpcb->p_nval = PM_ERR_PROFILE;
		    return 0;
		}

		if (shm_inst_n == 0) {
		    /* if no instances in the profile were matched, we fail */
		    __pmNotifyErr(LOG_ERR, "shm_fetch: no matching instances, indom=%d, pmid=%d", profp->profile[j].indom, pmid);
		    return PM_ERR_INST;
		}
		fetched = 1;
	    }

	    vpcb->p_nval = shm_inst_n;
	    sizepool(vpcb);

	    nval = 0;
	    for (j=0; j < shm_inst_n; j++) {
		switch (pmid) {
		    case PMID(1,29,1):		/* irix.ipc.shm.segsz */
#if (_MIPS_SZLONG == 32)
			av.l = (__int32_t)shm_inst[j].shm_segsz;
#elif (_MIPS_SZLONG == 64)
			av.ull = (__uint64_t)shm_inst[j].shm_segsz;
#else
    bozo
#endif
			break;
		    case PMID(1,29,2):		/* irix.ipc.shm.nattch */
			av.l = (__int32_t)shm_inst[j].shm_nattch;
			break;
		    default:
			__pmNotifyErr(LOG_WARNING, "shm_fetch: no case for PMID %s\n", pmIDStr(pmid));
			vpcb->p_nval = 0;
			return PM_ERR_PMID;
		}

                if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
                    return sts;
                vpcb->p_vp[nval].inst = shmid_list[j];
                if (nval == 0)
                    vpcb->p_valfmt = sts;
                nval++;
	    }
	    return 0;
	}
    }
    return PM_ERR_PMID;
}
