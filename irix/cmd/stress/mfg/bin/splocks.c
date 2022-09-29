/*
 * swlock -- 
 *
 * Test MP cache operations. Use the software semaphore to stress
 * caches.
 */
#include <malloc.h>
#include <ulocks.h>
#include <task.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/sysmp.h>
#include <sys/lock.h>
#include <sys/resource.h>
#include <sys/schedctl.h>

int cpu_done;
int slave_fail;
void slver_harder(void *);

static int Debug = 1 ;
#define NTIMES 10
#define WAIT_COUNT 2000
#define 	ONE_MEGLOCK	6096	/* take up 1 Meg of memory */
# define	MAXLOCK		40*ONE_MEGLOCK	
#define MAXCPU	36

#define eprintf	printf

/* spin lock */
typedef struct {
        int     lck[MAXCPU];
        int     last;
        int     lcnt;
        int     ucnt;
        int     lcallpc;
} mplock_t;
mplock_t sync_lock;		/* protect cpu_done */

struct tchk {
	int	lval;
	int	oval;
	mplock_t	*lp;
} *chklistarray, *chklist;
mplock_t	*splocks;
int idbg_debug_stop = 0;

int maxlocks = MAXLOCK;
int loop = 1;

int *cpu_id;
int cpunum;
#define spsema _spsema
#define svsema _svsema

usage()
{
	fprintf(stderr, "splocks [-m meg of memory] [-p # of processors]\n");
	exit(0);
}

system_sizing(int *procs, int *maxlocks)
{
	*procs = sysmp(MP_NAPROCS);
}
	
main(int argc, char **argv)
{
	int fail, board;
	extern int              optind;
        extern char             *optarg;
        int                     c;
        int                     err = 0;
	int 			memory = -1;
	int			procs = -1;
	struct rlimit		rlim;

	system_sizing(&cpunum, &maxlocks);
        /*
         * Parse arguments.
         */
        setbuf(stdout, NULL);
        while ((c = getopt(argc, argv, "m:p:l:")) != EOF) {
                switch (c) {
                case 'l':
                        if ((loop = strtol(optarg, (char **)0, 0)) <0)
                                err++;
                        break;
                case 'm':
                        if ((memory = strtol(optarg, (char **)0, 0)) <0)
                                err++;
                        break;
                case 'p':
                        if ((procs = strtol(optarg, (char **)0, 0)) <0)
                                err++;
                        break;
                default:
			err++;
			break;
		}
	}
	if (err)
		usage();
	printf("Original parameters: %d meg, %d processes\n", maxlocks/ONE_MEGLOCK, cpunum);
	if (memory != -1)
		maxlocks = memory*ONE_MEGLOCK;
	if (procs != -1)
		cpunum = procs;
	printf("New parameters: %d meg, %d processes\n", maxlocks/ONE_MEGLOCK, cpunum);
	getrlimit(RLIMIT_DATA, &rlim);
	rlim.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_DATA, &rlim) == -1) 
		perror("splocks");
	if (plock(PROCLOCK) == -1)
		perror("splocks");
	if (!(cpu_id = (int *)calloc(cpunum, sizeof(int))))
		perror("splocks");
	cpu_id[0] = getpid();
	if (!(splocks = (mplock_t *)calloc(maxlocks, sizeof(mplock_t))))
		perror("splocks");
	chklist = chklistarray = (struct tchk *)calloc(maxlocks, sizeof(struct tchk));
	if (!chklist)
		perror("splocks");
	usconfig(CONF_INITUSERS, cpunum);
	while (loop--) {
		fail = hardersema(cpunum);
		if ( fail == 0 )
			printf("\t\tPASSED.\n");
	}
}

hardersema(num)
int num;
{
	int fail;
	extern int sub_harder();

	fail = sub_harder(num);
	return(fail);
}

sub_harder(num)
int num;
{
	int id;
	int tmp, fail = 0;
	int data, i, k, addr;
	volatile int *p;
	register int (*func)();
	extern int h_idbg_lcktst();

	/* Init synchronization count and slave_fail */
	for (i = 0; i < maxlocks; i++) {
		chklist[i].lval = i;
		chklist[i].oval = 's';
		chklist[i].lp = (mplock_t *)&splocks[i];
	}

	spsema(&sync_lock);
	cpu_done = 0;
	svsema(&sync_lock);
	/* enable slave */
	for (i = 1; i <num; i++) {
		cpu_id[i] = sproc(slver_harder, PR_SADDR, &i);
		if (cpu_id[i] == -1)
			perror("splocks");
	}
#ifdef _RUN_ON
	sysmp(MP_MUSTRUN, 0);
#endif
#define _GANG	1
#ifdef _GANG
	if (schedctl(SCHEDMODE, SGS_GANG) == -1)
		perror("splocks");
#endif
	fail = h_idbg_lcktst();
	spsema(&sync_lock);
	cpu_done++;
	svsema(&sync_lock);
	fail |= wait_slave(num);
	return(fail|slave_fail);
}

/*
 * slver_harder
 *
 * 
 */
void
slver_harder(void *cpu)
{
#ifdef _RUN_ON
	sysmp(MP_MUSTRUN, *(int *)cpu);
#endif
	/* wait till pid is registered */
	while (cpu_id[*(int *)cpu] == 0)
		sginap(1);

	h_idbg_lckslv();
	/* indicate this one is done */
	slave_shar();
}

#define STOPIT

h_idbg_lcktst()
{
   int			i;
   long			cnt;
   int			fail = 0;

#if DEBUG
	if (Debug)
		printf("Master beginning lock tests\n");
#endif
	for (cnt = 0;cnt < NTIMES; cnt++) {
#if DEBUG
	if (Debug) {
		printf("Loop %d\n",cnt);
		printf("Going up\n");
	}
#endif
		for (i = 0; i < maxlocks; i++) {
			if (idbg_debug_stop) {
				printf("stopping before(up) lck:%x i:0x%x cnt:0x%x\n",
					chklist[i].lp, i, cnt);
				fail = 1;
				return(1);
			}
			spsema(chklist[i].lp);
#if DEBUG
			if (Debug)
				printf("Got lock %d\r",i);
#endif
			if (chklist[i].oval == 's') {
				if (chklist[i].lval != i) {
					STOPIT;
					fprintf(stderr,
"\nERROR: chklist[i].lval:0x%x  (!= i)(up) &chklist[i]:0x%x i:0x%x cnt:%d lock:0x%x\n",
						chklist[i].lval,
						&chklist[i],
						i, cnt,
						chklist[i].lp);
					fail = 1;
					return(1);
				}
				chklist[i].lval = maxlocks - chklist[i].lval;
				chklist[i].oval = 'm';
			}
			else
				if (chklist[i].lval != maxlocks - i) {
					STOPIT;
					fprintf(stderr,
"\nERROR: chklist[i].lval:0x%x (!= maxlocks - i)(up) &chklist[i]:0x%x i:0x%x cnt:%d lock:0x%x\n",
						chklist[i].lval,
						&chklist[i],
						i, cnt,
						chklist[i].lp);
					fail = 1;
					return(1);
				}
			svsema(chklist[i].lp);
		}
#if DEBUG
		if (Debug)
			printf("Going down\n");
#endif
		for (i = maxlocks - 1; i >= 0; i--) {
			if (idbg_debug_stop) {
				printf("stopping before(down) lck:%x i:0x%x cnt:0x%x\n",
					chklist[i].lp, i, cnt);
				fail = 1;
				return(1);
			}
			spsema(chklist[i].lp);
#if DEBUG
			if (Debug)
				printf("Got lock %d\r",i);
#endif
			if (chklist[i].oval == 's') {
				if (chklist[i].lval != i) {
					STOPIT;
					fprintf(stderr,
"\nERROR: chklist[i].lval:0x%x  (!= i)(down) &chklist[i]:0x%x i:0x%x cnt:%d lock:0x%x\n",
						chklist[i].lval,
						&chklist[i],
						i, cnt,
						chklist[i].lp);
					fail = 1;
					return(1);
				}
				chklist[i].lval = maxlocks - chklist[i].lval;
				chklist[i].oval = 'm';
			}
			else
				if (chklist[i].lval != maxlocks - i) {
					STOPIT;
					fprintf(stderr,
"\nERROR: chklist[i].lval:0x%x (!= maxlocks - i)(down) &chklist[i]:0x%x i:0x%x cnt:%d lock:0x%x\n",
						chklist[i].lval,
						&chklist[i],
						i, cnt,
						chklist[i].lp);
					fail = 1;
					return(1);
				}
			svsema(chklist[i].lp);
		}
#if NOTDEF
		printf(".");
		if ( cnt != 0 && !(cnt%60) )
			printf("\n");
#endif
	}
/*
	printf("\n");
*/
	return(fail);
}

h_idbg_lckslv()
{
   int			i;
   long			cnt;

#if DEBUG
	if (Debug)
		printf("Slave beginnning lock tests\n");
#endif
	for (cnt = 0;cnt < NTIMES; cnt++) {
#if DEBUG
		if (Debug) {
			printf("Loop %d\n",cnt);
			printf("Going down\n");
		}
#endif
		for (i = maxlocks - 1; i >= 0; i--) {
			if (idbg_debug_stop) {
				printf("stopping before(down) lck:%x i:0x%x cnt:0x%x\n",
					chklist[i].lp, i, cnt);
				slave_fail = 1;
				return(1);
			}
			spsema(chklist[i].lp);
#if DEBUG
			if (Debug)
				printf("Got lock %d\r",i);
#endif
			if (chklist[i].oval == 'm') {
				if (chklist[i].lval != maxlocks - i) {
					STOPIT;
					fprintf(stderr,
"\nERROR: chklist[i].lval:0x%x (!= maxlocks - i)(down) &chklist[i]:0x%x i:0x%x cnt:%d lock:0x%x\n",
						chklist[i].lval,
						&chklist[i],
						i, cnt,
						chklist[i].lp);
						slave_fail = 1;
					return(1);
				}
				chklist[i].lval = i;
				chklist[i].oval = 's';
			}
			else
				if (chklist[i].lval != i) {
					STOPIT;
					fprintf(stderr,
"\nERROR: chklist[i].lval:0x%x  (!= i)(down) &chklist[i]:0x%x i:0x%x cnt:%d lock:0x%x\n",
						chklist[i].lval,
						&chklist[i],
						i, cnt,
						chklist[i].lp);
					slave_fail = 1;
					return(1);
				}
			svsema(chklist[i].lp);
		}
#if DEBUG
		if (Debug)
			printf("Going up\n");
#endif
		for (i = 0; i < maxlocks; i++) {
			if (idbg_debug_stop) {
				printf("stopping before(up) lck:%x :0x%x cnt:0x%x\n",
					chklist[i].lp, i, cnt);
				slave_fail = 1;
				return(1);
			}
			spsema(chklist[i].lp);
#if DEBUG
			if (Debug)
				printf("Got lock %d\r",i);
#endif
			if (chklist[i].oval == 'm') {
				if (chklist[i].lval != maxlocks - i) {
					STOPIT;
					fprintf(stderr,
"\nERROR: chklist[i].lval:0x%x (!= maxlocks - i)(up) &chklist[i]:0x%x i:0x%x cnt:%d lock:0x%x\n",
						chklist[i].lval,
						&chklist[i],
						i, cnt,
						chklist[i].lp);
					slave_fail = 1;
					return(1);
				}
				chklist[i].lval = i;
				chklist[i].oval = 's';
			}
			else
				if (chklist[i].lval != i) {
					STOPIT;
					fprintf(stderr,
"\nERROR: chklist[i].lval:0x%x  (!= i)(up) &chklist[i]:0x%x i:0x%x cnt:%d lock:0x%x\n",
						chklist[i].lval,
						&chklist[i],
						i, cnt,
						chklist[i].lp);
					slave_fail = 1;
					return(1);
				}
			svsema(chklist[i].lp);
		}
#if NOTDEF
		printf(".");
		if ( cnt != 0 && !(cnt%60) )
			printf("\n");
#endif
	}
/*
	printf("\n");
*/
}

/*
 * slave_shar
 *
 * This is slave's shared routine. It will 
 * disable slave processor, bump synchronization count, and
 * finally jump to wait loop.
 */
slave_shar()
{
	/* bump synchronization count */

	spsema(&sync_lock);
	cpu_done++;
	svsema(&sync_lock);
}

/* 
 * wait_slave
 *
 * master spins until synchronization count is the specified number.
 */
wait_slave(num)
int num;
{
	int last_done = 0;
	while (cpu_done < num) {
		if (last_done != cpu_done) {
			printf("%d done!\r",cpu_done);
			last_done = cpu_done;
		}
		sginap(1);
	}
	return(0);
}


/*
 * Locking primitives.
 */
# define NUMCPUS	cpunum

_spsema(lck, callpc)
   register mplock_t	*lck;
   int			callpc;
{
	lock(lck, _cpuid(), callpc);
}

_svsema(lck, callpc)
   register mplock_t	*lck;
   int			callpc;
{
	unlock(lck, _cpuid(), callpc);
}
static
initlock(lck, v)
   register mplock_t	*lck;
   register int		v;
{
   register int		i;

	for (i = 0; i < NUMCPUS; i++)
		lck->lck[i] = 0;
	lck->last = 0;
	lck->lcnt = 0;
	lck->ucnt = 0;
}


lock(a, me, cpc)
   register mplock_t	*a;
   register int		me;
   register int		cpc;
{
   register int		i;
   register int		first;
   register int		ospl;
   int loopcnt = 0;
/*
	ospl = splhi();
	ASSERT((ospl & 0x1f00) == 0);

	ASSERT(a->lck[me] == 0);
*/
    loop:
	first = a->last;
	a->lck[me] = 1;
	loopcnt++;

    forloop:
	loopcnt++;
	if ( loopcnt > 2000000 )
	{
#ifdef DEBUG
		printf("requestor:%d time limit exceeded, lock addr:0x%x, owner:%d\n",me, a,a->last);
#endif
		loopcnt = 0;
		
	}
	for (i = first; i < cpunum; i++) {
		if (i == me) {
			a->lck[i] = 2;
			for (i = 0; i < cpunum; i++)
				if (i != me && a->lck[i] == 2)
					goto loop;
			a->last = me;
/*
			ASSERT(a->lcnt == a->ucnt);
*/
			a->lcnt++;
			a->lcallpc = cpc;
/*
			splx(ospl);
*/
			return;
		}
		else if (a->lck[i])
			goto loop;
	}
	first = 0;
	goto forloop;
}

unlock(a, me, cpc)
   register mplock_t	*a;
   register int		me;
   register int		cpc;
{
   register int		ospl;
/*
	ospl = splhi();
	ASSERT((ospl & 0x1f00) == 0);

	ASSERT(a->last == me);
	ASSERT(a->lck[me] == 2);
*/
	a->ucnt++;
/*
	ASSERT(a->lcnt == a->ucnt);
*/
	a->lcallpc = 0;
	a->last = (me + 1) % cpunum;
	a->lck[me] = 0;
/*
	splx(ospl);
*/
}

ownlock(a)
   register mplock_t	*a;
{
	return(a->lck[_cpuid()] == 2);
}

_cpuid()
{
	int pid;
	int i;

	/* convert pid into a sw cpuid */
	pid = get_pid();
	for (i=0; i<cpunum; i++) {
		if (pid == cpu_id[i])
			return(i);
	}
	eprintf("no match in _cpuid(), pid = %d", pid);
	exit(0);
}
