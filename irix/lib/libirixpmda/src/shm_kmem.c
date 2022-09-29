/*
 * Handle /dev/kmem data structures for ipc/shm
 *
 * Note: from IRIX 6.2 on, a struct shmid_ds has two different
 *	 definitions, one within the kernel and one above the
 *	 shmctl() interface ... this source file uses /dev/kmem
 *	 to access the former.  See also shm.c.
 */

#ident "$Id: shm_kmem.c,v 1.11 1997/11/21 06:34:30 kenmcd Exp $"

#define _KMEMUSER

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmp.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./shm.h"
#include "./cluster.h"

/* exported to indom.c and shm.c */
shm_map_t	*shm_map;
int		max_shm_inst;

#if defined(IRIX6_2) || defined(IRIX6_3)
#include "./kmemread.h"

/* IPC identifier */
#define ipc_id(slot, nslots, seq) (int)(slot + nslots * seq)

/*
 * Scan the kernel shm hash table for the current set of shm identifiers.
 * This is only called from indom.c: it's too expensive to do on every fetch.
 * Each fetch must have an explicit profile, from which the id's are
 * used to get the requested IPC_STAT info for the result. If an explicit
 * inclusion list of shm id's is not in the profile then the fetch will fail.
 */
int
reload_shm(void)
{
    int				i, count, sts;
    struct shmid_ds		probe;
    static int			first_time=1;
    static off_t		shmds_addr = (off_t)0;
    static struct shmid_ds	**shmds;

    if (first_time) {
	off_t			shminfo_addr;
	struct shminfo		shminfo;
	if ((shminfo_addr = sysmp(MP_KERNADDR, MPKA_SHMINFO)) == -1 || !VALID_KMEM_ADDR(shminfo_addr)) {
	    __pmNotifyErr(LOG_ERR, "reload_shm: failed to get address of shminfo: %s\n", pmErrStr(-errno));
	    goto FAIL;
	}

	if ((sts = kmemread(shminfo_addr, &shminfo, (int)sizeof(shminfo))) != sizeof(shminfo)) {
	    __pmNotifyErr(LOG_ERR, "reload_shm: failed to read shminfo: %s\n", pmErrStr(-errno));
	    goto FAIL;
	}
	max_shm_inst = shminfo.shmmni;

	/*
	 * Allocate an array of pointers (actually kernel off_t's) for the shm seg table.
	 * The shmid's are hashed into this table but only the key_t is stored.
	 * So to get the id's we have to use id = shmget(key, 0, 0).
	 */
	if (shmds_addr == (off_t)0 && (shmds_addr = sysmp(MP_KERNADDR, MPKA_SHM)) == -1 || !VALID_KMEM_ADDR(shmds_addr)) {
	    __pmNotifyErr(LOG_ERR, "reload_shm: failed to get address of shm: %s\n", pmErrStr(-errno));
	    return -errno;
	}

	if ((shmds = (struct shmid_ds **)malloc(max_shm_inst * sizeof(struct shmid_ds *))) == NULL) {
	    __pmNotifyErr(LOG_ERR, "reload_shm: failed to malloc %d shm pointers: %s\n", max_shm_inst, pmErrStr(-errno));
	    goto FAIL;
	}

	if ((shm_map = (shm_map_t *)malloc(max_shm_inst * sizeof(shm_map_t))) == NULL) {
	    __pmNotifyErr(LOG_ERR, "reload_shm: failed to malloc %d shm_map_t instances: %s\n", max_shm_inst, pmErrStr(-errno));
	    goto FAIL;
	}

	first_time=0;
    }

    if (kmemread(shmds_addr, shmds, max_shm_inst * (int)sizeof(struct shmid_ds *)) < 0) {
	__pmNotifyErr(LOG_ERR, "reload_shm: failed to read shm seg table: %s\n", pmErrStr(-errno));
	return -errno;
    }

    for (count=i=0; i < max_shm_inst; i++) {
        if (shmds[i] != NULL && VALID_KMEM_ADDR((off_t)(shmds[i]))) {
            sts = kmemread((off_t)shmds[i], &probe, (int)sizeof(struct shmid_ds));
            if (sts < 0 || !(probe.shm_perm.mode & IPC_ALLOC)) {
		/*
		 * Invalid entry for one reason or another
		 * This flags that shm_map[i] is not a valid entry.
		 */
		shm_map[i].id = -1;
	    }
	    else {
		count++;
		shm_map[i].id = ipc_id(i, max_shm_inst, probe.shm_perm.seq);
		shm_map[i].key = probe.shm_perm.key;
	    }
        } else
            shm_map[i].id = -1;
    }
    return count;

FAIL:
    return 0;
}

#else /* 6.4 or later */
#if defined (IRIX6_4)

#define IDBLOCK 10
int
reload_shm(void)
{
	struct shmstat	shmstat;
	int i, j;

	shmstat.sh_id = 0;
	shmstat.sh_location = -1LL;
	i = 0;
	while (sysmp(MP_SHMSTAT, &shmstat, sizeof(struct shmstat)) == 0) {
		if (i >= max_shm_inst) {
			if ((shm_map = (shm_map_t *)realloc(shm_map,
					(max_shm_inst+IDBLOCK) * sizeof(shm_map_t))) == NULL) {
			    __pmNotifyErr(LOG_ERR, "reload_shm: failed to realloc %d shm_map_t instances: %s\n", max_shm_inst+IDBLOCK, pmErrStr(-errno));
			    goto FAIL;
			}
			max_shm_inst += IDBLOCK;
		}
		shm_map[i].id = shmstat.sh_id;
		shm_map[i].key = shmstat.sh_shmds.shm_perm.key;
		i++;
	}
	for (j = i; j < max_shm_inst; j++)
		shm_map[j].id = -1;
	return i;

FAIL:
	return 0;
}

#else	/* IRIX6_5 */

extern int	__shmstatus(struct shmstat *);

#define IDBLOCK 10
int
reload_shm(void)
{
    struct shmstat	shmstat;
    int			rv, j, i = 0;

    shmstat.sh_id = 0;
    shmstat.sh_location = -1LL;
    while (((rv = __shmstatus(&shmstat)) == 0) || (errno == EACCES)) {
	if (!rv && (errno == EACCES)) {
	    /* can't see it but it exists */
	    continue;
	}

	if (i >= max_shm_inst) {
	    if ((shm_map = (shm_map_t *)realloc(shm_map,
		    (max_shm_inst+IDBLOCK) * sizeof(shm_map_t))) == NULL) {
		__pmNotifyErr(LOG_ERR,
			"reload_shm: failed to realloc %d shm_map_t instances: %s\n",
			max_shm_inst+IDBLOCK, pmErrStr(-errno));
		return 0;
	    }
	    max_shm_inst += IDBLOCK;
	}
	shm_map[i].id = shmstat.sh_id;
	shm_map[i].key = shmstat.sh_shmds.shm_perm.key;
	i++;
    }
    for (j = i; j < max_shm_inst; j++)
	shm_map[j].id = -1;
    return i;
}

#endif /* IRIX_6.5 */
#endif
