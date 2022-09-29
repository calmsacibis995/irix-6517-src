/*
 * $Id: dsk.c,v 1.1 1994/08/05 01:02:52 tin Exp $
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <sys/times.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "dsk.h"

#define VERSION		"v1.0"
static char *dsk_rcsid = "$Id: dsk.c,v 1.1 1994/08/05 01:02:52 tin Exp $";

typedef char	ARR[MAXTASKS][STRLEN];

ARR	dkarr, *pdkarr;

/* for shared mem, sharing dkarr with child procs */
int	dkshmid = 0;
key_t	dkkey = 100;
int	dkshmflg = (IPC_CREAT | 0666);

int	kids = 0;			/* number of users forked */
int	children = 1;
int	running = 0;
int	debug=0;
int	workload=100;

typedef void (*FPtr)(int);
typedef int (*Tptr)(char *);
typedef struct {
	Tptr	f;
	char	*s;
	int	wgt;
} Tstruct;

/* Declare functions */
void diskunl_all(void);
int diskw_all(void);
void math_err(int sig);
void dead_kid(int sig);
void killall(int sig);
void go(void);
int fork_(void);
void Usage(char *prog);

/* test functions */
int creat_clo(char *);
int disk_rr(char *);
int disk_rw(char *);
int disk_rd(char *);
int disk_cp(char *);
int disk_wr(char *);
int dsearch(char *);

Tstruct tasks[] = {
			creat_clo,	"creat_clo", 100,
			disk_rr,	"disk_rr", 100,
			disk_rw,	"disk_rw", 150,
			disk_rd,	"disk_rd", 100,
			disk_cp,	"disk_cp", 100,
			disk_wr,	"disk_wr", 300,
			dsearch,	"dsearch", 150,
			NULL,		NULL, 1000
};
#define NTASKS	(sizeof(tasks) / sizeof(Tstruct))

void main(int argc, char **argv)
{
	int	i, n, c, status;
	FPtr	sigvalu;
	struct rlimit rl;
	char	*addr;

	printf("%s %s\n", argv[0], VERSION);

	(void)setsid();
	(void)signal(SIGINT,SIG_IGN);	/* catch signal and ignore */
	(void)signal(SIGTERM,killall);	/* child process problem */

	/* Increase limits */
	if (getrlimit(RLIMIT_FSIZE, &rl) == -1) {
		perror("getrlimit");
		fprintf(stderr, "getrlimit(RLIMIT_FSIZE) failed!\n");
		exit(1);
	}
	if (rl.rlim_cur != RLIM_INFINITY) {
		rl.rlim_max = rl.rlim_cur = RLIM_INFINITY;
		if (setrlimit(RLIMIT_FSIZE, &rl) == -1) {
			perror("setrlimit");
			fprintf(stderr,
				"setrlimit Failed: current = %ld, max = %ld\n",
				rl.rlim_cur, rl.rlim_max);
			exit(1);
		}
	}

	while ((c = getopt(argc, argv, "n:d:w:V")) != EOF)
		switch(c) {
		case 'n':	/* num of child */
			children = atoi(optarg);
			children = (children < 1 ? 1 : children);
			break;
		case 'w':	/* work load */
			workload = atoi(optarg);
			workload = (workload < 1 ? 100 : workload);
			break;
		case 'd':	/* debug level */
			debug = atol(optarg);
			debug = ((debug<1 || debug>100) ? 1 : debug);
			break;
		case 'V':	/* Version */
			fprintf(stderr, "%s %s\n", argv[0], VERSION);
			fprintf(stderr, "%s\n", dsk_rcsid);
			exit(0);
			break;
		default:
			Usage(argv[0]);
			break;
		}

	if (debug > 5)
		printf("debug=%d, workload=%d, children=%d\n",
			debug, workload, children);
	printf("\n...testing started\n\n");
	fflush(stdout);

	/* create and attach shm segment to dkarr[] */
	if ((dkshmid = shmget(dkkey, sizeof(dkarr), dkshmflg)) == -1) {
		perror("shmget");
		exit(1);
	}
	if (debug > 5)
		fprintf(stderr,
			"dkshmid=%d, sizeof(dkarr)=%d\n",
			dkshmid, sizeof dkarr);

	if (debug > 5)
		fprintf(stderr, "addr=0x%x, dkarr=0x%x, pdkarr=0x%x\n",
			addr, dkarr, pdkarr);
	if ((addr=shmat(dkshmid, NULL, SHM_RND)) == (void *)-1) {
		perror("shmat");
		exit(1);
	}
	pdkarr = (ARR *)addr;
	if (debug > 5)
		fprintf(stderr, "addr=0x%x, dkarr=0x%x, pdkarr=0x%x\n",
			addr, dkarr, pdkarr);

	/* fork child procs, but don't start yet... */
	for (i=0; i<children; i++) {
		dowork(i);
	}
	memcpy(pdkarr, dkarr, sizeof(dkarr));

	/*
	 * create the disk files that all disk_rd and disk_cp tests
	 * will use. 
	 */
	if(diskw_all() == -1) {
		perror("diskw_all()");
		fprintf(stderr, "disk work file creation failed .. EXIT\n");
		fflush(stderr);
		killall(0);
	}

	/* Now start! */
	printf("BANG! Off they go!\n");
	kill(0, SIGINT);

	while (1) {
		if (wait(&status) > 0) {
			if ((status & 0377) == 0177) { /* child proc stopped */
				fprintf(stderr,
				"\nChild process stopped by signal #%d\n",
				((status >> 8) & 0377));
			}
			else if ((status & 0377) != 0) { /* child term by sig */
				if ((status & 0377) & 0200) {
					fprintf(stderr,"\ncore dumped\n");
					fprintf(stderr,
					   "\nChild terminated by signal #%d\n",
					   (status & 0177));
				}
			}
			else { /* if ((status & 0377)==0) ** child exit()'ed */
				if (((status >> 8) & 0377))
					fprintf(stderr,
					   "\nChild process called exit(), status = %d\n",
						((status >> 8) & 0377));
			}
		--kids;
		if (kids == 0) break;
		}
		else if (errno == ECHILD) break;
	}
	/* 
	 * unlink disk files used by disk_rd and disk_cp tests
	 */
	diskunl_all();

	/* detach shm mem */
	if ((int)shmdt(addr) == -1) {
		perror("shmdt");
		exit(1);
	}

        /* remove dkshmid */
	if ((int)shmctl(dkshmid, IPC_RMID, NULL) == -1) {
		perror("shmctl(IPC_RMID)");
		exit(1);
	}

	fflush(stdout);
	exit(0);
}


dowork(int child)
{
	int	j, k, randnum;
	int	wlist[NTASKS], wlrs;
	int	childpid;
	double	x;
	char	buf[STRLEN];

	/* init workload array */
	for (j=0, wlrs=0; j<NTASKS; j++) {
		x = ((double)tasks[j].wgt/tasks[NTASKS].wgt) * workload;
		wlist[j] = (int)x;
		if ((x - (double)wlist[j]) >= 0.5) {
			wlist[j]++;
			wlrs++;
		} else if ((x - (double)wlist[j]) != 0.0) {
			if (wlist[j] == 0) {
				wlist[j] = 1;
				wlrs++;
			} else
				wlrs--;
		}
	}

	/* fork off a child process */
	if ((childpid = fork_()) == 0)  {	/* if we are child */
		srand(child);
		srand(child);
		signal(SIGINT, go);
		signal(SIGTERM, SIG_DFL);
		signal(SIGHUP, dead_kid);
		signal(SIGFPE, math_err);

		/* wait for signal, so everyone start together */
		pause(); 

		for (j=0; j<workload; j++) {
			errno = 0;

			/* sampling without replacement */
			randnum = rand();
			k = randnum % NTASKS;
			if (wlist[k] > 0)
				wlist[k]--;
			else {
				k = reduce_list(wlist);
				wlist[k]--;
			}

			if (debug && tasks[k].s)
				fprintf(stderr, "calling function %s\n",
					tasks[k].s);

			if ( tasks[k].s && (*tasks[k].f)((*pdkarr)[child]) == -1 ) {
				perror(buf);
				fprintf(stderr,
					"\nFailed to execute\n\t%d\n", k);
				kill(getppid(), SIGTERM);
				exit(1);
			}
		}
		exit(0);
	}

	/* check for and make work dir if needed */
	sprintf(dkarr[child], "%d", childpid);
	if (access(dkarr[child], R_OK|W_OK|X_OK) == -1) {
		if (errno == ENOENT) {
			if (mkdir(dkarr[child], 0777) == -1) {
				perror("mkdir");
				fprintf(stderr, "mkdir(%s)\n",
					dkarr[child]);
				exit(1);
			}
		} else  {
			perror("access(R_OK|W_OK|X_OK)");
			fprintf(stderr, "unable to access %s\n",
				dkarr[child]);
			exit(1);
		}
	}
}

int fork_(void)
{
	int k, fk;

	/* try up to 10 tries */
	for (k=0; k<10; k++)
		if( (fk = fork()) < 0)  {
			sleep(1);
		}
		else {
			++kids;
			return (fk);
		}

	/* if fork fails */
	killall(0);
	return(-1);
}

void go(void)
{
	running = 1;
}

/*
 *  killall sends signals to all child process and waits
 *  for their death
 */
void killall(int sig)
{
	signal(SIGTERM, SIG_IGN);
	diskunl_all();
	if (sig == 0)
	  fprintf(stderr, "Fatal Error! SIGTERM (#%d) received!\n\n", sig);

	kill(0, SIGTERM);
	while (wait((int *)0) != -1) ; /* wait for all users to die */
	exit(0);
}

void dead_kid(int sig)
{
	int status;

	signal(sig, SIG_IGN);
	diskunl_all();
	fprintf(stderr, "\ndead_kid() received signal SIGHUP (%d)\n", sig);
	if (wait(&status) > 0) {
		if ((status & 0377) == 0177) { /* child proc stopped */
			fprintf(stderr,
				"Child process stopped on signal = %d\n",
				((status >> 8) & 0377));
		}
		else if ((status & 0377) != 0) { /* child term by sig */
			fprintf(stderr,
				"Child terminated by signal = %d\n",
				(status & 0177));
			if ((status & 0377) & 0200)
				fprintf(stderr,"core dumped\n");
		}
		else { /* if ((status & 0377) == 0) ** child exit()'ed */
			fprintf(stderr,
				"Child process called exit(), status = %d\n",
				((status >> 8) & 0377));
		}
	}
	kill(getppid(), SIGTERM);
	exit(1);
}

void math_err(int sig)
{
	int	status;

	signal(sig,SIG_IGN);
	fprintf(stderr, "\nmath_err() received signal SIGFPE (%d)\n", sig);
	fprintf(stderr, "Floating Point Exception error\n");
	if (wait(&status) > 0) {
		if ((status & 0377) == 0177) { /* child proc stopped */
			fprintf(stderr,
				"Child process stopped on signal = %d\n",
				((status >> 8) & 0377));
		}
		else if ((status & 0377) != 0) { /* child term by sig */
			fprintf(stderr,
				"Child terminated by signal = %d\n",
				(status & 0177));
			if ((status & 0377) & 0200)
				fprintf(stderr,"core dumped\n");
		}
		else { /* if ((status & 0377) == 0) ** child exit()'ed */
			fprintf(stderr,
				"Child process called exit(), status = %d\n",
				((status >> 8) & 0377));
		}
	}
	kill(getppid(),SIGTERM);
	exit(1);
}

/*
 * write out the work files that disk_rd and disk_cp use.
 */

#include <fcntl.h>

#define BUFCOUNT	100
#define BUFNBLKS	8

char BUF[BUFNBLKS*512];		/* 4KB */
char fn1[STRLEN];
char fn1arr[MAXTASKS][STRLEN];

int diskw_all(void)
{
	int fd1, j, k;

	for (j=0; j<(BUFNBLKS*512); j++)
		BUF[j] = (char)(j%127);
	if(children > 1) {
		for(j=0; j<children; j++) {
			sprintf(fn1, "%s/%s", dkarr[j], TMPFILE1);
			if((fd1 = creat(fn1,0666)) == -1) {
				perror("creat");
				fprintf(stderr, "1. diskw_all: can't create %s\n",fn1);
				return(-1);
			}
			k = BUFCOUNT;
			while(k--) {
				if(write(fd1,BUF,sizeof BUF) != sizeof BUF) {
					perror("write");
					fprintf(stderr,
					"2. diskw_all: can't create %s (%d)\n",
						fn1, sizeof BUF);
					close(fd1);
					return(-1);
				}
			}
			strcpy(fn1arr[j], fn1);
			close(fd1);
		}
	}
	else {
		sprintf(fn1,"%s",TMPFILE1);
		if((fd1 = creat(fn1,0666)) == -1) {
			perror("creat");
			fprintf(stderr, "3. diskw_all: can't create %s\n",fn1);
			return(-1);
		}
		k = BUFCOUNT;
		while(k--) {
		if(write(fd1,BUF,sizeof BUF) != sizeof BUF) {
			perror("write");
			fprintf(stderr, "4. diskw_all: can't create %s\n",fn1);
			close(fd1);
			return(-1);
		}
		}
		strcpy(fn1arr[0],fn1);
		close(fd1);
	}
	if (debug)
		fprintf(stderr,
			"diskw_all: children=%d, created %s(%d)\n",
			children, fn1, BUFCOUNT);
	return(0);
}

/*
 * get rid of 'em 
 */
void diskunl_all(void)
{
	int  j;

	if(children > 0) {
		for(j=0; j<children; j++) {
			unlink(fn1arr[j]);
		}
	}
	else {
		unlink(fn1arr[0]);
	}
}

void Usage(char *prog)
{
	fprintf(stderr,
		"Usage: %s [-V] [-d n] [-n procs] [-w workload]\n",
		prog);
	exit(1);
}

/*
 * reduce_list()
 *      Sampling without replacement.
 */
int reduce_list(int wlist[])
{
	register int	i, total;
	int		rlist[NTASKS];

	for (i=0, total=0; i<NTASKS; i++) {
		if (wlist[i] == 0)
			continue;
		else
			rlist[total++] = i;
	}
	if (total)
		i = rand() % total;
	else {
		fprintf(stderr, "FATAL ERROR - DIVIDE BY ZERO\n");
		fprintf(stderr, "reduce_list(): total = %d\n", total);
		exit(1);
	}
	return(rlist[i]);
}

