#define  _KMEMUSER
#include <sys/types.h>
#include <sys/reg.h>
#undef _KMEMUSER

#include <ulocks.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include <nanothread.h>

#include <unistd.h>
#include <stdlib.h>
#include <sys/sysmp.h>
#ifdef STRESS
#include <sys/mman.h>
#endif /* STRESS */

#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <assert.h>

#include <stddef.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syssgi.h>

#include "mp.h"
void bclr(char *bp, bitnum_t b);
int btst(char *bp, bitnum_t b);

#define NPARBLOCKS	16
#define NTHREADS 	10
#define EXITCODE	58
#define MINTHREADS	1
#define MAXTHREADS	128

#define USEFUL		(char) '1'
#define	IDLE		(char) '2'
#define LOCKWAIT	(char) '3'
#define GO		(char) '4'
#define YIELD		(char) '5'
#define DONE		(char) '6'
#define ITERDONE	(char) '7'

#define DURATION	100000

#define SLAVEIDFIRST	2

#define RESUME		0x678
#define SPIN		0x737

/* Accessing high resolution clock */
typedef unsigned long long iotimer_t;
static 	__psunsigned_t phys_addr, raddr;
static 	unsigned int cycleval;
int 	fd, poffmask;
volatile iotimer_t counter_value, *iotimer_addr;

#define GET_TIMER()	*iotimer_addr
#define CYCLE_TO_MICRO(x)	((x) * 21 / 1000)
#define CYCLE_TO_NANO(x)	((x) * 21)


typedef struct {
	volatile char 	state;
	volatile int	niters;
	volatile int 	totaliters;
	iotimer_t	idletime;
	iotimer_t	usefultime;
} evcontrol_t;

static volatile evcontrol_t evcontrol[MAXTHREADS];

static volatile int alldone = 0;
static volatile int nyield = 0;

int time_yield = 0;
static volatile int ok_to_yield[MAXTHREADS];
volatile int srcpid[MAXTHREADS], destpid[MAXTHREADS];
iotimer_t start_yield[MAXTHREADS], end_yield[MAX_THREADS];

extern int yields[], fyields[];

static usema_t *usema;
static usptr_t *appusp;

#ifdef DEBUG
#define dprintf(x)	printf x
#else
#define dprintf(x)
#endif /* DEBUG */

/*
 * can be modified by sugnumthd
 */
int	suggestednthreads;

static
char *
evstate_to_string(char state)
{
	switch(state) {
	case USEFUL: 	return("U");
	case IDLE:   	return("I");
	case LOCKWAIT:	return("LW");
	case GO:	return("G");
	case YIELD: 	return("Y");
	case DONE: 	return("D");
	case ITERDONE: 	return("ID");
	default:	return("UNKNOWN");
	}
}


static void
init_usema(int nthreads)
{
	char		usaname[16];

	sprintf(usaname, "usa%05d.app", getpid());
	if (usconfig(CONF_INITUSERS, nthreads+1) == -1) {
		perror("usconfig");
		exit(-1);
	}
	/* arrange for the arena to be removed on exit of all procs */
	if (usconfig(CONF_ARENATYPE, US_SHAREDONLY) == -1) {
		perror("usconfig");
		exit(-1);
	}
	appusp = usinit(usaname);
	if (appusp == NULL) {
		perror("usinit");
		exit(-1);
	}
	if ((usema = usnewsema(appusp, 1)) == NULL) {
		perror("usnewsema");
		exit(-1);
	}
}


int
fsb(uint64_t * mask, int evids[])
{
	register int i;
	register int j;

	j = 0;
	for (i = 0;  i <= MAXTHREADS; i++) {
		if (mask[i/64] & (1LL << (i % 64)))
			evids[j++] = i;
	}
	evids[j] = -1;
	return(j);
}


int
check_status(int prevnallocated)
{
        int  cnallocated;
	static int  np = 0;
	int evids[MAXTHREADS], nevids, i;

        cnallocated = kusp->nallocated;

        if (cnallocated < prevnallocated) {

		nevids = fsb(kusp->rbits, evids);
		printf("\t");
                if (nevids) {
                        printf(" [P");
			for (i = 0; i < nevids; i++)
				printf(" %d(%s)", evids[i],
					evstate_to_string(evcontrol[i].state));
			printf(", N=%d]", cnallocated);
		} else
			printf(" [P ??, N=%d]", cnallocated);
        } else if (cnallocated > prevnallocated) {

		nevids = fsb(kusp->rbits, evids);

                if (nevids) {
                        printf(" [R");
			for (i = 0; i < nevids; i++)
				printf(" %d(%s)", evids[i],
					evstate_to_string(evcontrol[i].state));
			printf(", N=%d]", cnallocated);
		} else
			printf(" [R ??, N=%d]", cnallocated);
        }
	/* if (((++np) % 6) == 0) */
	if (cnallocated != prevnallocated) {
		printf("\n");
		fflush(stdout);
	}
	return(cnallocated);
}


void
lock(void)
{
	if (uspsema(usema) == -1) {
		perror("uspsema");
		exit(-1);
	}
}

void
unlock(void)
{
	if (usvsema(usema) == -1) {
		perror("usvsema");
		exit(-1);
	}
}

pid_t
junkcall()
{
	return(PRDA->sys_prda.prda_sys.t_pid);
}

iotimer_t
wait_till_go_or_done(register int32_t me, void *arg)
{
	register int j, n;
	int	 mask;
	iotimer_t t, t1, t2;

	t = GET_TIMER();
	while (evcontrol[me].state != GO && !alldone) {
		if (time_yield)
			continue;
		/*
		 * See if anybody needs help.
		 */
		if (arg == (void *)SPIN)
			continue;
		assert(arg == (void *)RESUME);
		if (kusp->nrequested == kusp->nallocated)
			continue;

		/* Some thread is in trouble, see if it needs help */
		/*
		 * Count the no. of threads in trouble.
		 */
		mask = *(int *)kusp->rbits;
		bclr((char *) &mask, 0);  /* clear out master, just in case */

		for (n=0, j=SLAVEIDFIRST; mask; bclr((char *)&mask, j), j++)
		{
			if (btst((char *) &mask,j) && (evcontrol[j].state==GO))
			{
				t1 = GET_TIMER();
				resume_nid(me,kusp,j);
				t2 = GET_TIMER();
				t += t2 - t1;	/* In case we resumed "j" */
				break; /* out of for */
			}
		}
	}
	return(GET_TIMER() - t);
}


void
child(void *arg)
{
	int		niters, k, i;
	register int32_t	myevid;
	iotimer_t	t;
	int		result = 0;
	struct		prda_sys *prdasys;

	if ((prdasys = (struct prda_sys *) schedctl(SETHINTS)) == ((struct prda_sys *)-1L)) {
		printf("error in SETHINTS acquiring prda\n");
	}
	myevid = prdasys->t_nid;

	dprintf(("Child (pid = %d, evid= %d), got to run, arg = 0x%x!\n",
		getpid(), myevid, arg));


	while ( !alldone ) {
	  if (time_yield) {
		if (evcontrol[myevid].state != GO)
			continue;
		t = GET_TIMER();
		niters = evcontrol[myevid].niters;
		dprintf(("Child (pid = %d, evid= %d), niters = %d\n",
			getpid(), myevid, niters));
		/* all odd numbered RSAs will delay until some other PID
		 * resumes the RSA.
		 */
		if ((myevid & 0x1)) {
			ok_to_yield[myevid] = 1;
retry1:
			while (ok_to_yield[myevid] == 1) {
			  junkcall();
			}
			while (ok_to_yield[myevid] != 2) {
			  junkcall();
			}
			destpid[myevid] = PRDA->sys_prda.prda_sys.t_pid;
			end_yield[myevid] = GET_TIMER();
			while (ok_to_yield[myevid] == 2) {
			  if (!resume_nid(myevid, kusp, myevid-1)) {
				/* bad resume - should NOT occur */
			  	printf("ERROR - BAD RE-YIELD, %d:%d\n",
				       myevid, myevid-1);
			  }
			}
			if (ok_to_yield[myevid] == 1)
				goto retry1;
			
		} else {
			if (ok_to_yield[myevid+1] == 1) {
				srcpid[myevid+1] =PRDA->sys_prda.prda_sys.t_pid;
				start_yield[myevid+1] = GET_TIMER();
				ok_to_yield[myevid+1] = 2;
				if (!resume_nid(myevid, kusp, myevid+1)) {
				  ok_to_yield[myevid+1] = 1;
				  end_yield[myevid+1] = GET_TIMER();
				  printf("YIELD BAD to rsa %d (from %d) yield time %d (%d microsec)\n",
					 myevid+1, myevid,
					 end_yield[myevid+1]-start_yield[myevid+1],
					 ((cycleval/1000) * (end_yield[myevid+1]-start_yield[myevid+1]))/1000);
				  continue;
				} else {
				  /* successful yield */
				  ok_to_yield[myevid+1] = 0;
				  printf("spid %d dpid %d YIELD OK to rsa %d (from %d) yield time %d (%d microsec)\n",
					 srcpid[myevid+1], destpid[myevid+1],
					 myevid+1, myevid,
					 end_yield[myevid+1]-start_yield[myevid+1],
					 ((cycleval/1000) * (end_yield[myevid+1]-start_yield[myevid+1]))/1000);
				}
			} else
				continue;
		}
	      } else {		/* !TIME_YIELD */
		if (evcontrol[myevid].state != GO)
			continue;
		t = GET_TIMER();
		niters = evcontrol[myevid].niters;
		dprintf(("Child (pid = %d, evid= %d), niters = %d\n",
			getpid(), myevid, niters));
		for (k = 0; k < niters; k++) {
			for (i = 0; i < 1000; i++) {
				/*
				 * do some trivial computation that the
				 * compiler won't optimize out
				 */
				result += i * k;
			}
		}
	      }	/* !TIME_YIELD */

		evcontrol[myevid].totaliters += niters;
		evcontrol[myevid].state = ITERDONE;
		evcontrol[myevid].usefultime += GET_TIMER() - t;

	  if (time_yield)
		t = wait_till_go_or_done(myevid, arg);
	  else
		while (evcontrol[myevid].state != GO && !alldone) {
		  ;
		}

		evcontrol[myevid].idletime += t;
	}
	exit(result);
}

main(int argc, char **argv)
{
	int 		i;
	int 		nallocated = 0;
	int		nthreads;
	struct timespec rqtp, rmtp;
	int		ctotal,  totaliters, niters;
	char		sched;
	int		schedcode;
	int		nproc, ndone;
	int		currnthreads;
	iotimer_t 	t, t0;
	double		totaltime;


	if (argc != 3) {
		printf("Usage: app <nthreads> <sched>\n");
		if (argc == 1) {
			printf("ASSUME:  app 19 t\n");
			nthreads = 19;
			sched = 't';
		} else
			exit(-1);
	} else {
		nthreads = atoi(argv[1]) + 1; /* 1 for master,rest for the slave*/
		sched = argv[2][0];
	}

	srandom(getpid());

	if (nthreads < MINTHREADS || nthreads > MAXTHREADS) {
		printf("No. of threads between {%d, %d}\n",
			MINTHREADS, MAXTHREADS);
		exit(-1);
	}

	if ((nproc = sysconf(_SC_NPROC_ONLN)) == -1) {
		perror("_SC_NPROC_ONLN");
		exit(-1);
	}
	printf("Total number of processors = %d\n", nproc);
	if (sched == 'r') {
		printf("************* Will do resuming scheduling\n");
		schedcode = RESUME;
	} else if (sched == 's') {
		printf("************* Will do spinning scheduling\n");
		schedcode = SPIN;
	} else if (sched == 't') {
		printf("************* Time user level context switch\n");
		schedcode = RESUME;
		time_yield = 1;
	} else {
		printf("Sched has to be 'r' or 's' or 't'\n");
		exit(-1);
	}
	
	printf("Parent, mypid = %d\n", getpid());

	for (i = SLAVEIDFIRST; i < nthreads+SLAVEIDFIRST-1; i++) {
		evcontrol[i].state = IDLE;
		evcontrol[i].totaliters = 0;
		evcontrol[i].idletime = 0;
		evcontrol[i].usefultime = 0;
	}

	poffmask = getpagesize() - 1;
	phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
	raddr = phys_addr & ~poffmask;
	fd = open("/dev/mmem", O_RDONLY);
	iotimer_addr = (volatile iotimer_t *)mmap(0, poffmask, PROT_READ,
				MAP_PRIVATE, fd, (off_t)raddr);
	iotimer_addr = (iotimer_t *)((__psunsigned_t)iotimer_addr +
				(phys_addr & poffmask));

	if (time_yield) {
		printf("Parent: will restrict to run on processor %d\n", nproc-1);

		if (sysmp(MP_MUSTRUN, nproc-1) == -1) {
			perror("MP_MUSTRUN");
			exit(-1);
		}
		printf("Parent: restricted to run on processor %d\n", nproc-1);
	}
	
	init_usema(nthreads);
	set_num_processors(nthreads);
	start_threads(child, (void *)schedcode);

	if (schedcode == SPIN) {
		printf("initilizing sugnumthd\n");

		suggestednthreads = currnthreads = nthreads;
		if (schedctl(SCHEDMODE, SGS_GANG, 0) < 0) {
			perror("schedctl(SCHDMODE, SGS_GANG)");
			exit(-1);
		}
		/* __mp_sugnumthd_init(1,nthreads,nthreads); */
	} else {
#ifdef RESTRICT
		printf("Parent: will restrict to run on processor %d\n", nproc);

		if (sysmp(MP_MUSTRUN, nproc) == -1) {
			perror("MP_MUSTRUN");
			exit(-1);
		}
		printf("Parent: restricted to run on processor %d\n", nproc);
#endif /* RESTRICT */
		suggestednthreads = currnthreads = nthreads;
	}

	nallocated = kusp->nallocated;

	rqtp.tv_sec = 1;
	rqtp.tv_nsec = 100000; /* 1 milliseconds */

	t0 = GET_TIMER();

	totaliters = 0;
	for(i = 0; i < NPARBLOCKS; i++) {
		int j;
		
		niters = DURATION + (random() % DURATION);
		dprintf(("---------------- Parallel Block %d (%d) -----------------\n", i, niters));


		/*
		 * adjust number of threads
		 */
		if (currnthreads > suggestednthreads) {
			dprintf(("Blocking threads ... "));
			for (j = suggestednthreads; j < currnthreads; j++) {
#ifdef NOTYET
				block_vp(j); 
#endif
				dprintf((" %d", j));
			}
			dprintf(("\n"));
		} else if (currnthreads < suggestednthreads) {
			dprintf(("Unblocking threads ... "));
			for (j = currnthreads; j < suggestednthreads; j++) {
#ifdef NOTYET
				unblock_vp(j);
#endif
				dprintf((" %d", j));
			}
			dprintf(("\n"));
		}
		/* In any case, currnthreads == suggestednthreads now */
		currnthreads = suggestednthreads;
		dprintf(("Current Threads = %d\n", currnthreads));

		for (j = SLAVEIDFIRST; j < currnthreads+SLAVEIDFIRST-1; j++)
			evcontrol[j].niters = niters/(currnthreads - SLAVEIDFIRST);

		for (j = SLAVEIDFIRST; j < currnthreads+SLAVEIDFIRST-1; j++)
			evcontrol[j].state = GO;


		while (1) {
			ndone = 0;
			for (j = SLAVEIDFIRST; j < currnthreads+SLAVEIDFIRST-1; j++)
				if (evcontrol[j].state == ITERDONE ||
				    evcontrol[j].state == YIELD)
					ndone++;
			if (ndone == (currnthreads - SLAVEIDFIRST +1))
				break; /* out of while */
			

#ifdef RESUME_RESTRICT
			if (schedcode == RESUME) {
				suggestednthreads = ((suggestednthreads + kusp->nallocated) / 2 + 1); 
				if (suggestednthreads > nthreads)
					suggestednthreads = nthreads;
			}
#endif /* RESUME_RESTRICT */

			nallocated = check_status(nallocated);
			if (nanosleep(&rqtp, &rmtp) == -1) {
				printf("Whoa! nanosleep interrupted!!\n");
				perror("nanosleep");
			}
		}
		totaliters += niters;
	}
	alldone = 1;
	if (schedcode == SPIN)
		__mp_sugnumthd_exit();
#ifdef NOTYET
	for (i = currnthreads; i < nthreads; i++)
		unblock_vp(i); 
#endif

	ctotal = 0;
	for (i = SLAVEIDFIRST ; i < nthreads+SLAVEIDFIRST-1; i++)
		ctotal += evcontrol[i].totaliters;

	t = GET_TIMER();

	totaltime = CYCLE_TO_NANO(t - t0);

	printf("[%lld => %lld] (%lld)\n", t0, t, (t - t0));

	if (schedcode == RESUME) {
		int ny, nfy;

		printf("Directed Yields:\n");
		printf("\tEVID\t\tSUCESS\tFAIL\n");
		ny = nfy = 0;
		for (i = SLAVEIDFIRST; i < currnthreads+SLAVEIDFIRST-1; i++) {
			printf("\t%d\t\t%d\t%d\n", i, yields[i], fyields[i]);
			ny += yields[i];
			nfy += fyields[i];
		}
		fprintf(stderr,
			"TotalIter:%d:PerIter:%6.4f:Yield=SUCCESS/FAIL:%d:%d\n",
			totaliters, totaltime / totaliters, ny, nfy);
		printf("Total iterations according to childern = %d\n", ctotal);
	} else {
		fprintf(stderr, "TotalIter:%d:PerIter:%6.4f\n",
			totaliters, totaltime / totaliters);
		printf("Total iterations according to childern = %d\n", ctotal);
	}
	fprintf(stderr, "ITERATION TIMES (microseconds):\n");
	for (i = SLAVEIDFIRST; i < currnthreads+SLAVEIDFIRST-1; i++) {
		double usefultime, idletime; 

		usefultime = evcontrol[i].usefultime;
		idletime = evcontrol[i].idletime;

		fprintf(stderr, "\tUSEFUL: %lld IDLE: %lld RATIO: %6.2f GRAIN: %lld\n", 
			CYCLE_TO_MICRO(evcontrol[i].usefultime),
			CYCLE_TO_MICRO(evcontrol[i].idletime),
			(usefultime / (usefultime + idletime)) * 100.0,
			CYCLE_TO_MICRO(evcontrol[i].usefultime) / NPARBLOCKS);
	}
}
