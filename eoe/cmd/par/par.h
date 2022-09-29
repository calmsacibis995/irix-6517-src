
#include <sys/types.h>
#include <sys/par.h>

#define PADC	"padc"
#define NAMELEN 16

/*
 * We use an egregiously large hash table size in order to handle cases where
 * we may be keeping track of hundreds or even thousands of processes.  Since
 * pids tend to clump together we probably will only use a few cache lines
 * out of each table when we're working with a small number of processes.
 */
#define PIDHASHSIZE  4096			/* must be power of 2 */
#define PIDHASH(pid) ((pid) & (PIDHASHSIZE-1))	/* hash pid to hash index */

#define MAXPIDS 1000

typedef __uint64_t ptime_t;		/* time base in par */

extern	ptime_t	tstampdivms;	/* time stamp divisor for ms conversion */
#define	TOMS(t)	((ptime_t)(t) / tstampdivms)
extern	ptime_t	tstampdivus;	/* time stamp divisor for us conversion */
#define	TOUS(t)	((ptime_t)(t) / tstampdivus)

/* system call link list */
struct cl {
	pid_t		pid;
	int		prstart;	/* if set then we have printed start */
	ushort		call;
	int		cpuid;
	char		abi;
	int		num;
	ptime_t		tick;
	usysarg_t	parameters[FAWLTY_MAX_PARAMS];
	struct subsyscall_s *sys;
	struct cl	*hnext;	/* hash chain */
	struct indirect *ip;	/* list of data associated w/ indirect args */
	struct indirect *lip;	/* ptr to last one */
	struct cl	*endrec;/* ptr to endcall record */
};

struct indirect {
	int		argn;	/* argument number */
	int		len;	/* length in bytes of additional info */
	char		*data;	/* ptr to data */
	struct indirect *next;
};

/* process name table */
struct pn {
	pid_t		pid;
	char		name[NAMELEN];
	struct pn	*hnext;	/* hash chain */
};

struct proclist {		/* linked list of processes */
 	struct proclist* next;
	struct proclist* prev;
};
struct procq {			/* queue of processes */
	struct proclist h;	/* list of processes */
	int qlen;		/* queue length */
	int qmaxlen;		/* max queue length over time */
};
/* process statistics table */
struct procrec {
	struct proclist h;	/* queue links */
	struct procq *curq;	/* current q process is on */
	struct procrec *hnext;	/* hash chain */
	pid_t pid;		/* process/thread id */
	int pri;
	int rtpri;
	int lastrun;
	int slices;
	ptime_t starttick;
	ptime_t runtime;
	int queued;
	int lqueued;
	ptime_t queuedtick;
	ptime_t queuedtime;
	ptime_t lqueuedtime;
	ptime_t sleeptick;
	ptime_t sleeptime;
	int cpuswitch;
	int preempt_short;
	int preempt_long;
	int swtch;
	int sleep;
	int yield;
	int mustrun;
};

/* cpu statistics table */
struct cpurec {
	ptime_t idletimes;	/* total number of times went idle */
	ptime_t idleticks;	/* total mS spent idle */
	ptime_t wentidle;	/* tick number last went idle */
	int disps;		/* total times dispatched */
	int hotdisps;		/* times dispatched from local/affinity q */
	ptime_t runticks;	/* total mS spent running */
	int swtchs;	/* number of calls to swtch */
	pid_t curpid;	/* running pid */
	pid_t lastpid;	/* last running pid */
	int runsame;	/* times called swtch but ran same guy */
	int nothighest;	/* ran guy other than top of run q */
	int curpri;	/* priority of currently running proc at dispatch */
};

/* "Exported" Interfaces */
struct procrec *procfind(pid_t);
