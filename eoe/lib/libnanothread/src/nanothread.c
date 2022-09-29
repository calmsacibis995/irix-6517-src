#define _KMEMUSER
#include <sys/types.h>
#undef _KMEMUSER
#include <sys/prctl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <assert.h>
#include <bstring.h>
#include <ulocks.h>
#include <stdlib.h>
#include <sys/syssgi.h>

#ifndef NANOTHREADS_VERSION1
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif /* NANOTHREADS_VERSION1 */

#include "nanothread.h"

/*
 * Internal to the library
 */
#define PR_CNOSCHED	0x08000000		/* from sys/prctl.h 	*/


#ifdef DEBUG
#define dprintf(x)	printf x;
#else
#define dprintf(x)
#endif


typedef struct {
	nanostartf_t	startf;
	void		*arg;
	int		vpid;
} child_desc_t;




static int		nvpid = 1; 	/* next vpid to assign, master is 0 */
static usema_t 		*vpidsema = NULL;
static usptr_t		*usp = NULL;
static pid_t		vptab[1024]; 	/* to keep spids 	*/

/*
 * Exported interfaces
 */
kusharena_t 	*kusp = NULL;

int
set_num_processors(int n)
{
	if (n < 1)
		return -1;

	if (kusp == NULL) {
#ifdef NANOTHREADS_VERSION1
		kusp = (kusharena_t *) syssgi(SGI_ALLOCSHARENA);
		if (kusp == (void *) -1) {
			perror("syssgi");
			exit(-1);
		}
#else
		int 	fd;
		long 	len = sysconf(_SC_PAGESIZE);

		if ((fd = open("/dev/zero", O_RDWR)) < 0) {
			perror("open:/dev/zero");
			exit(-1);
		}
		if ((kusp = (kusharena_t *) mmap(0, len, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0)) == (kusharena_t *) MAP_FAILED)
		{
			kusp = NULL;
			perror("mmap:/dev/zero");
			exit(-1);
		}
		if (syssgi(SGI_ALLOCSHARENA, kusp) == -1) {
			perror("ALLOCSHARENA");
			munmap(kusp, len);
			exit(-1);
		}
#endif
		printf("Allocated SHARENA @ 0x%llx\n", kusp);
		kusp->nrequested = n;
		return 0;
	} else {
		printf("set_num_processors: changing nrequested not implemented\n");
		return -1;
	}
}


static void
libstartf(void *arg)
{
	child_desc_t	cd;

	cd = *(child_desc_t *)arg;
	free(arg); 			/* was malloc()'ed by parent */

	dprintf(("[%d], @ libstartf, vpid = %d\n", getpid(), cd.vpid));
	if (syssgi(SGI_SETVPID,cd.vpid)) {
		printf("[%d], error in SETVPID\n", getpid());
		perror("SGI_SETVPID");
		exit(-1);
	}
	dprintf(("[%d], SETVPID done, calling 0x%x\n", getpid(), cd.startf));
	(*cd.startf)(cd.arg);
}


int
start_threads(nanostartf_t startf, void *arg)
{
	child_desc_t	*cdp;
	register pid_t	cpid;
	register int	i;
	char		usaname[16];

	if (kusp == NULL)
		return -1;
	/*
 	 * Grab the vpidsema to serialize, allocate arena/lock if necessary.
	*/
	if (vpidsema == NULL) {
		dprintf(("[%d], allocating vpidsema\n", getpid()));
		/* allocate ...  */
		assert(usp == NULL);
		assert(nvpid == 1);
		sprintf(usaname, "usa%05d", getpid());
		/* Initialize to proper number of sprocs to share */
		/* FIX: +4 gives some room for master thread, etc ...
		 * make it more visible as part of interface?
		 */
		if (usconfig(CONF_INITUSERS, kusp->nrequested+4) == -1) {
			perror("usconfig");
			exit(-1);
		}
		/* arrange for the arena to be removed on exit of all procs */
		if (usconfig(CONF_ARENATYPE, US_SHAREDONLY) == -1) {
			perror("usconfig");
			exit(-1);
		}
		usp = usinit(usaname);
		if (usp == NULL) {
			perror("usinit");
			exit(-1);
		}
		dprintf(("[%d], allocated usarena\n", getpid()));
		if ((vpidsema = usnewsema(usp, 1)) == NULL) {
			perror("usnewsema");
			exit(-1);
		}
		dprintf(("[%d], usnewsema successful\n", getpid()));
	}
	dprintf(("[%d], at uspsema\n", getpid()));
	if (uspsema(vpidsema) == -1) {
		perror("uspsema");
		exit(-1);
	}

	dprintf(("[%d], will start %d sprocs\n", getpid(), kusp->nrequested));
	for (i = 1; i < kusp->nrequested; i++, nvpid++) {

		cdp = (child_desc_t *) malloc(sizeof(child_desc_t));
		if (cdp == NULL) {
			perror("malloc for child descriptor");
			exit(-1);
		}
		cdp->startf = startf;
		cdp->arg = arg;
		cdp->vpid = nvpid;

		/* cdp will be free()'ed by child */
		cpid = sproc(libstartf, PR_SALL, (void *) cdp);
		switch (cpid) {
		   case 0:
			/* child */
			perror("sproc:child");
			exit(-1);
		   case -1:
			perror("sproc");
			break;
		   default:
			/* parent */
			vptab[i] = cpid;
			break;
		}
		dprintf(("[%d], started %dth sproc (vpid = %d)\n", getpid(), i, nvpid));
	}

	if (syssgi(SGI_SETVPID,0)) { /* master vpid is 0 */
		printf("[%d], error in SETVPID, master\n", getpid());
		perror("SGI_SETVPID");
		exit(-1);
	}

	if (usvsema(vpidsema) == -1) {
		perror("usvsema");
		exit(-1);
	}
	return 0;
}

/* ARGSUSED */
int
resume_vp(vpid_t rvpid)
{
	return((int) sginap(0));
}

vpid_t
getvpid()
{
	return((vpid_t) syssgi(SGI_GETVPID));
}

void
block_vp(vpid_t vpid)
{
	blockproc(vptab[vpid]);
}

void
unblock_vp(vpid_t vpid)
{
	unblockproc(vptab[vpid]);
}
