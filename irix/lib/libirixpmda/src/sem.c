/*
 * Handle metrics for ipc/sem (31)
 */

#ident "$Id: sem.c,v 1.18 1997/11/21 06:34:26 kenmcd Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmp.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

static pmMeta		meta[] = {
/* irix.ipc.sem.nsems */
  { 0, { PMID(1,31,1), PM_TYPE_32, PM_INDOM_SEMSET, PM_SEM_INSTANT,  {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.ipc.sem.ncnt */
  { 0, { PMID(1,31,2), PM_TYPE_U32, PM_INDOM_SEM, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.ipc.sem.zcnt */
  { 0, { PMID(1,31,3), PM_TYPE_U32, PM_INDOM_SEM, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } }
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		sema_fetched = 0;
static int		sema_n;
static int		sema_max = 0;
static struct semid_ds	*sema = (struct semid_ds *)0;
static int		semaset_fetched = 0;
static int		semaset_n;
static int		semaset_max = 0;
static struct semid_ds	*semaset = (struct semid_ds *)0;
static int		sema_inst_max = 0;
static int		*sema_inst = (int *)0;

/* exported to indom.c */
int			semset_ctr = 0;

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
#include "./kmemread.h"

/* exported to indom.c */
struct semid_ds         *semds_instance;
struct seminfo          seminfo;

static off_t		seminfo_addr;
static off_t		semds_addr = (off_t)0;
struct semid_ds         **semds;

/*
 * Scan the kernel shm hash table for the current set of shm identifiers.
 * This is only called from indom.c: its too expensive to do on every fetch.
 * Each fetch must have an explicit profile, from which the ids are
 * used to get the requested IPC_STAT info for the result. If an explicit
 * inclusion list of shm ids is not in the profile then the fetch will fail.
 */
int
reload_sem(void)
{
    int i, count, sts;
    static int first_time=1;

    if (first_time) {
	if ((seminfo_addr = sysmp(MP_KERNADDR, MPKA_SEMINFO)) == -1 || !VALID_KMEM_ADDR(seminfo_addr)) {
	    __pmNotifyErr(LOG_ERR, "reload_sem: failed to get address of seminfo: %s\n", pmErrStr(-errno));
	    goto FAIL;
	}

	if ((sts = kmemread(seminfo_addr, &seminfo, (int)sizeof(seminfo))) != sizeof(seminfo)) {
	    __pmNotifyErr(LOG_ERR, "reload_sem: failed to read seminfo: %s\n", pmErrStr(-errno));
	    goto FAIL;
	}

	/*
	 * Allocate an array of pointers (actually kernel off_ts) for the sem seg table.
	 * The semids are hashed into this table but only the key_t is stored.
	 * So to get the ids we have to use id = semget(key, 0, 0).
	 */
	if (semds_addr == (off_t)0 && (semds_addr = sysmp(MP_KERNADDR, MPKA_SEM)) == -1 || !VALID_KMEM_ADDR(semds_addr)) {
	    __pmNotifyErr(LOG_ERR, "reload_sem: failed to get address of sem: %s\n", pmErrStr(-errno));
	    return -errno;
	}

	if ((semds = (struct semid_ds **)malloc(seminfo.semmni * sizeof(struct semid_ds *))) == (struct semid_ds**)0) {
	    __pmNotifyErr(LOG_ERR, "reload_sem: failed to malloc %d sem pointers: %s\n", seminfo.semmni, pmErrStr(-errno));
	    goto FAIL;
	}

	if ((semds_instance = (struct semid_ds *)malloc(seminfo.semmni * sizeof(struct semid_ds))) == (struct semid_ds *)0) {
	    __pmNotifyErr(LOG_ERR, "reload_sem: failed to malloc %d sem instances: %s\n", seminfo.semmni, pmErrStr(-errno));
	    goto FAIL;
	}

	first_time=0;
    }

    if (kmemread(semds_addr, semds, seminfo.semmni * (int)sizeof(struct semid_ds *)) < 0) {
	__pmNotifyErr(LOG_ERR, "reload_sem: failed to read sem seg table: %s\n", pmErrStr(-errno));
	return -errno;
    }

    semset_ctr = 0;
    for (count=i=0; i < seminfo.semmni; i++) {
        if (semds[i] != (struct semid_ds *)0 && VALID_KMEM_ADDR((off_t)(semds[i]))) {
            sts = kmemread((off_t)semds[i], &semds_instance[i], (int)sizeof(struct semid_ds));
            if (sts < 0 || !(semds_instance[i].sem_perm.mode & IPC_ALLOC)) {
		/*
		 * Invalid entry for one reason or another
		 * This flags that semds_instance[i] is not a valid entry.
		 */
		semds[i] = (struct semid_ds *)0;
	    }
	    else {
		count += semds_instance[i].sem_nsems;
		semset_ctr++;
	    }
        } else
            semds[i] = (struct semid_ds *)0;
    }
    return count;

FAIL:
    return 0;
}

#else /* IRIX6_5 */

#include "./sem.h"

/* exported to indom.c */
sem_set	*semset_list = NULL;	/* list of known semaphore sets */
int	semset_mark = 0;	/* high water mark - space for semset_list */
int	sem_ctr = 0;		/* total number of semaphores across all sets */

int
reload_sem(void)
{
    int			sts;
    struct semstat	semstat;

    semset_ctr = sem_ctr = 0;
    semstat.sm_id = 0;
    semstat.sm_location = -1LL;
    while (((sts = __semstatus(&semstat)) == 0) || (errno == EACCES)) {
	if (!sts && (errno == EACCES))
	    continue;

	if (semset_ctr >= semset_mark) {
	    if ((semset_list = (sem_set *)realloc(semset_list,
				++semset_mark * sizeof(sem_set))) == NULL) {
		__pmNotifyErr(LOG_ERR, "reload_sem: realloc failed, %d bytes: %s\n", (semset_mark * sizeof(sem_set)), pmErrStr(-errno));
		return 0;
	    }
	}
	semset_list[semset_ctr].set_id = semstat.sm_id;
	semset_list[semset_ctr].set_key = semstat.sm_semds.sem_perm.key;
	semset_list[semset_ctr].set_nsems = semstat.sm_semds.sem_nsems;
	sem_ctr += semstat.sm_semds.sem_nsems;
	semset_ctr++;
    }
    return sem_ctr;
}

#endif	/* IRIX6_5 */


void
irixpmda_sem_init(int reset)
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
	    __pmNotifyErr(LOG_ERR, "sem_init: bad instance domain (%d) for metric %s\n",
			 indomtag, pmIDStr(meta[i].m_desc.pmid));
	    continue;
	}
	/* Replace tag with its indom */
	meta[i].m_desc.indom = indomtab[indomtag].i_indom;
    }

    return;
}

int
irixpmda_sem_desc(pmID pmid, pmDesc *desc)
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
irixpmda_sem_fetch_setup(void)
{
    indomtab[PM_INDOM_SEM].i_numinst = reload_sem();

    sema_fetched = semaset_fetched = 0;
    sema_n = semaset_n = 0;
}

int
irixpmda_sem_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    int			j;
    int			k;
    int			e;
    int			id;
    int			sts;
    int			nval;
    int			profile_found;
    pmAtomValue		av;
    union semun		semun;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    if (pmid == PMID(1,31,1)) {
		/*
		 * irix.ipc.sem.nsems
		 * instance domain is "semaphore sets"
		 */
		if (semaset_fetched == 0) {
		    /*
		     * Extract the instance list (i.e. IPC keys) from the
		     * supplied profile.
		     * The profile MUST be an explicit inclusion list
		     * (i.e. exclude all, include some)
		     */

		    for (profile_found=0, j=0; j < profp->profile_len; j++) {

			if (profp->profile[j].indom != meta[i].m_desc.indom) {
			    continue;
			}

			if (profp->profile[j].state != PM_PROFILE_EXCLUDE ||
			    profp->profile[j].instances_len == 0)
			    break;

			profile_found++;
			for (k=0; k < profp->profile[j].instances_len; k++) {
			    /*
			     * Extract the requested semid_ds instances.
			     * Ignore any non-existing instances.
			     */
			    if (semaset_n >= semaset_max) {
				/* get some more space! */
				semaset_max += 8;
				if ((semaset = (struct semid_ds *)realloc(semaset,
				    semaset_max * sizeof(struct semid_ds))) == (struct semid_ds *)0) {
				    e = -errno;
				    __pmNotifyErr(LOG_ERR, "sem_fetch: failed to realloc %d semid_ds semaset entries : %s\n", semaset_max, pmErrStr(-errno));
				    return e;
				}
			    }

			    if (semaset_n >= sema_inst_max) {
				/* get some more space! */
				sema_inst_max += 8;
				if ((sema_inst = (int *)realloc(sema_inst, sema_inst_max * sizeof(int))) == (int *)0) {
				    e = -errno;
				    __pmNotifyErr(LOG_ERR, "sem_fetch: failed to realloc %d semid_ds sema_inst entries : %s\n", sema_inst_max, pmErrStr(-errno));
				    return e;
				}
			    }

			    id = profp->profile[j].instances[k];
			    semun.buf = &semaset[semaset_n];
			    if ((sts = semctl(id, 0, IPC_STAT, semun)) < 0) {
				continue;
			    }
			    sema_inst[semaset_n++] = id;
			}
		    }

		    if (profile_found == 0) {
			if (indomtab[PM_INDOM_SEM].i_numinst == 0)
			    vpcb->p_nval = PM_ERR_VALUE;
			else
			    vpcb->p_nval = PM_ERR_PROFILE;
			return 0;
		    }
		    semaset_fetched = 1;
		}

		vpcb->p_nval = semaset_n;
		sizepool(vpcb);

		nval = 0;
		for (j=0; j < semaset_n; j++) {
		    av.l = semaset[j].sem_nsems;
		    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
			return sts;
		    vpcb->p_vp[nval].inst = sema_inst[j];
		    if (nval == 0)
			vpcb->p_valfmt = sts;
		    nval++;
		}
	    }
	    else {
		/*
		 * irix.ipc.sem....
		 * instance domain is "semaphores"
		 */
		if (sema_fetched == 0) {
		    /*
		     * Extract the instance list (i.e. IPC keys) from the
		     * supplied profile.
		     * The profile MUST be an explicit inclusion list
		     * (i.e. exclude all, include some)
		     */

		    for (profile_found=0, j=0; j < profp->profile_len; j++) {

			if (profp->profile[j].indom != meta[i].m_desc.indom) {
			    continue;
			}

			if (profp->profile[j].state != PM_PROFILE_EXCLUDE ||
			    profp->profile[j].instances_len == 0)
			    break;

			profile_found++;
			for (k=0; k < profp->profile[j].instances_len; k++) {
			    /*
			     * Extract the requested semid_ds instances.
			     * Ignore any non-existing instances.
			     */
			    if (sema_n >= sema_max) {
				/* get some more space! */
				sema_max += 8;
				if ((sema = (struct semid_ds *)realloc(sema,
				    sema_max * sizeof(struct semid_ds))) == (struct semid_ds *)0) {
				    e = -errno;
				    __pmNotifyErr(LOG_ERR, "sem_fetch: failed to realloc %d semid_ds sema entries : %s\n", sema_max, pmErrStr(-errno));
				    return e;
				}
			    }

			    if (sema_n >= sema_inst_max) {
				/* get some more space! */
				sema_inst_max += 8;
				if ((sema_inst = (int *)realloc(sema_inst, sema_inst_max * sizeof(int))) == (int *)0) {
				    e = -errno;
				    __pmNotifyErr(LOG_ERR, "sem_fetch: failed to realloc %d semid_ds sema_inst entries : %s\n", sema_inst_max, pmErrStr(-errno));
				    return e;
				}
			    }

			    id = profp->profile[j].instances[k];
			    semun.buf = &sema[sema_n];
			    if ((sts = semctl(id >> 16, id & 0xffff, IPC_STAT,
					      semun)) < 0) {
				continue;
			    }

			    sema_inst[sema_n++] = id;
			}
		    }

		    if (profile_found == 0) {
			vpcb->p_nval = PM_ERR_PROFILE;
			return 0;
		    }
		    sema_fetched = 1;
		}

		vpcb->p_nval = sema_n;
		sizepool(vpcb);

		nval = 0;
		for (j=0; j < sema_n; j++) {
		    switch (pmid) {
			case PMID(1,31,2):		/* irix.ipc.sem.ncnt */
			    if ((sts = semctl(sema_inst[j] >> 16, sema_inst[j] & 0xffff, GETNCNT)) >= 0)
				av.l = sts;
			    else
				return -errno;
			    break;
			case PMID(1,31,3):		/* irix.ipc.sem.zcnt */
			    if ((sts = semctl(sema_inst[j] >> 16, sema_inst[j] & 0xffff, GETZCNT)) >= 0)
				av.l = sts;
			    else
				return -errno;
			    break;
			default:
			    __pmNotifyErr(LOG_WARNING, "sem_fetch: no case for PMID %s\n", pmIDStr(pmid));
			    vpcb->p_nval = 0;
			    return PM_ERR_PMID;
		    }

		    if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
			return sts;
		    vpcb->p_vp[nval].inst = sema_inst[j];
		    if (nval == 0)
			vpcb->p_valfmt = sts;
		    nval++;
		}
	    }
	    return 0;
	}
    }
    return PM_ERR_PMID;
}
