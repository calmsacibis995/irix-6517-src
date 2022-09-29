/*
 * top - a top users display for Unix
 *
 * SYNOPSIS:  Any SGI machine running IRIX 6.2 and up (*)
 *
 * DESCRIPTION:
 * This is the machine-dependent module for IRIX
 *
 * CFLAGS: -DHAVE_GETOPT -D_OLD_TERMIOS -DORDER
 *
 * * PREKUDZU: Older systems rely on /dev/kmem and have no native threads.
 *	This ugly ifdef highlights the differences; default is undefined.
 *
 * AUTHOR: Sandeep Cariapa <cariapa@sgi.com>
 * AUTHOR: Larry McVoy <lm@sgi.com>
 * Sandeep did all the hard work; I ported to 6.2 and fixed up some formats.
 * AUTHOR: John Schimmel <jes@sgi.com>
 * He did the all irix merge.
 * AUTHOR: Ariel Faigon <ariel@sgi.com>
 *	Ported to Ficus/Kudzu (IRIX 6.4+).
 *	Got rid of all nlist and different (elf64, elf32, COFF) kernel
 *	dependencies
 *	Various small fixes and enhancements: multiple CPUs, nicer formats.
 *	Added -DORDER process display ordering
 *	cleaned most -fullwarn'ings.
 *	Need -D_OLD_TERMIOS when compiling on IRIX 6.4 to work on 6.2 systems
 *	Support much bigger values in memory sizes (over Peta-byte)
 */

#define _KMEMUSER

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/sysinfo.h>
#include <sys/sysmp.h>
#include <sys/utsname.h>
#include <sys/schedctl.h>	/* for < 6.4 NDPHIMAX et al. */
#ifndef	PREKUDZU
#include <sys/sysget.h>
#endif
#include <paths.h>
#include <assert.h>
#include <values.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/resource.h>

#include "top.h"
#include "machine.h"

#define KMEM	"/dev/kmem"

typedef double load_avg;
#define loaddouble(la) (la)
#define intload(i) ((double)(i))

extern char	do_threads;	/* for thread support */

/*
 * Structure for keeping track of CPU times from last time around
 * the program.  We keep these things in a hash table, which is
 * recreated at every cycle.
 */
struct oldproc {
	pid_t	oldpid;
#ifndef	PREKUDZU
	tid_t	oldtid;
#endif	/* PREKUDZU */
	double	oldtime;
	double	oldpct;
};
static int oldprocs;                    /* size of table */
static struct oldproc *oldbase;
#define HASH(x) ((x << 1) % oldprocs)


#define pagetok(pages) ((((uint64_t) pages) * pagesize) >> 10)

/*
 * Ugly hack, save space and complexity of allocating and maintaining
 * parallel arrays to the prpsinfo array: use pr_psargs[]
 * in prpsinfo structures to store %CPU calculated values
 */
#define D_align(addr)		(((unsigned long)(addr) + 0xf) & ~0x0fU)
#define percent_cpu(pp)		(((double *) D_align(&((pp)->pr_psargs)))[0])
#define weighted_cpu(pp)	(((double *) D_align(&((pp)->pr_psargs)))[1])


/* Username field to fill in starts at: */
#define UNAME_START 22
#define THREAD_START 17

/*
 *  These definitions control the format of the per-process area
 */
static char header[] =
"       PID       PGRP X        PRI  SIZE   RES STATE    TIME WCPU% CPU% COMMAND";
/*
 012345678901234567890123456789012345678901234567890123456789012345678901234567
          10        20        30        40        50        60        70
 */

/*        PID PGRP USER  PRI   SIZE  RES   STATE  TIME %WCPU  %CPU  CMD */
#define Proc_format \
	"%10d %10d %-8.8s%4.4s %5.5s %5.5s %-6.6s %6.6s %4.4s %5.5s %-.7s"


/*
 * these are for detailing the cpu states
 * Data is taken from the sysinfo structure (see <sys/sysinfo.h>)
 * We rely on the following values:
 *
 *	#define CPU_IDLE        0
 *	#define CPU_USER        1
 *	#define CPU_KERNEL      2
 *	#define CPU_WAIT        3
 *	#define CPU_SXBRK       4
 *	#define CPU_INTR        5
 */
#ifndef CPU_STATES	/* defined only in 6.4 and up */
# define CPU_STATES 6
#endif

int	cpu_states[CPU_STATES];
char	*cpustatenames[] = {
	"idle", "usr", "ker", "wait", "xbrk", "intr",
	NULL
};

/* these are for detailing the memory statistics */

#define MEMSTATS 10
int	memory_stats[MEMSTATS];
char	*memorynames[] = {
	"K max, ", "K avail, ", "K free, ", "K swap, ", "K free swap", NULL
};

char	uname_str[40];
double	load[3];
static  char fmt[MAX_COLS + 2];
int	numcpus;

/* useful externals */
extern int	errno;
extern char	*sys_errlist[];

extern char	*myname;
extern char	*format_k();
extern char	*format_time();
extern long	percentages();

static int kmem;
static unsigned long avenrun_offset;

static float	irix_ver;		/* for easy numeric comparison */

static struct prpsinfo	*pbase;
static struct prpsinfo	**pref;
static struct oldproc	*oldbase;
static int		oldprocs;	/* size of table */

static DIR	*procdir;

static int	ptable_size;	/* allocated process table size */
static int	nproc;		/* estimated process table size */
static int	pagesize;

/* get_process_info passes back a handle.  This is what it looks like: */
struct handle {
	struct prpsinfo **next_proc;	/* points to next valid proc pointer */
	int		remaining;	/* number of pointers remaining */
};

static struct handle	handle;

void getptable(struct prpsinfo *baseptr);
void size(int fd, struct prpsinfo *ps);
void allocate_proc_tables();

extern char *ordernames[];

/*
 * Process states letters are mapped into numbers
 * 6.5 seems to have changed the semantics of prpsinfo.pr_state
 * so we rely, (like ps does) on the char value pr_sname.
 * The order we use here is what may be most interesting
 * to top users:  Most interesting state on top, least on bottom.
 * 'S' (sleeping) is the most common case so I put it _after_
 * zombie, even though it is more "active" than zombie.
 *
 * State letters and their meanings:
 *
 *	R   Process is running (may not have a processor yet)
 *	I   Process is in intermediate state of creation
 *	X   Process is waiting for memory
 *	T   Process is stopped
 *	Z   Process is terminated and parent not waiting (zombie)
 *	S   Process is sleeping, waiting for a resource
 */

/* abbreviated process states */
static char *state_str[] =
{ "", "sleep", "zombie", "stop", "swap", "start", "ready", "run", NULL };

/* Same but a little "wordier", used in CPU activity summary */
int     process_states[8];	/* per state counters */
char	*procstatenames[] = {
	/* ready to run is considered running here */
	"",		" sleeping, ",	" zombie, ",	" stopped, ",
	" swapped, ",	" starting, ",	" ready, ",	" running, ",
	NULL
};

#define S_RUNNING	7
#define S_READY		6
#define S_STARTING	5
#define S_SWAPPED	4
#define S_STOPPED	3
#define S_ZOMBIE	2
#define S_SLEEPING	1

#define IS_ACTIVE(pp) \
	(first_screen ? proc_state(pp) >= S_STARTING : percent_cpu(pp) > 0.0)

/*
 * proc_state
 *	map the pr_sname value to an integer.
 *	used as an index into state_str[]
 *	as well as an "order" key
 */
static int proc_state(struct prpsinfo *pp)
{
    char psname = pp->pr_sname;

    switch (psname) {
	case 'R': return
		 (pp->pr_sonproc >= 0 && pp->pr_sonproc < numcpus) ?
			S_RUNNING /* on a processor */ : S_READY;
	case 'I': return S_STARTING;
	case 'X': return S_SWAPPED;
	case 'T': return S_STOPPED;
	case 'Z': return S_ZOMBIE;
	case 'S': return S_SLEEPING;
	default : return 0;
    }
}


/*
 * To avoid nlist'ing the kernel (with all the different kernel type
 * complexities), we estimate the size of the needed working process
 * table by scanning  /proc/pinfo and taking the number of entries
 * multiplied by some reasonable factor.
 * Assume current dir is _PATH_PROCFSPI
 */
static int proc_entry_count()
{
	DIR	*dirp;
	int	pcnt;

	if ((dirp = opendir(".")) == NULL) {
		(void) fprintf(stderr, "%s: Unable to open %s\n",
					myname, _PATH_PROCFSPI);
		exit(1);
	}
	for (pcnt = 0; readdir(dirp) != NULL; pcnt++)
		;
	closedir(dirp);

	return pcnt;
}

/*
 * allocate space for:
 *	proc structure array
 *	array of pointers to the above (used for sorting)
 *	array for storing per-process old CPU usage
 */
void
allocate_proc_tables()
{
	int	n_entries = proc_entry_count();
	struct	rlimit	limits;

	if (pbase != NULL)  /* && n_active < ptable_size */
		return;

	/*
	 * Preallocate more than the current number of processes
	 * if we overflow later, we restart top
	 */
	/* add spaces for thread if we are doing threads */ 
	if (do_threads) {	
		if (getrlimit( RLIMIT_PTHREAD, &limits) == -1) {
			 fprintf(stderr, "top: getrlimit error =%d\n", errno);
			 exit(1);
		}
		nproc = (n_entries *3) + (n_entries * limits.rlim_cur);
	}
	else	nproc = n_entries * 3;

	oldprocs = 2 * nproc;

	pbase = (struct prpsinfo *)
		malloc(nproc * sizeof(struct prpsinfo));
	pref = (struct prpsinfo **)
		malloc(nproc * sizeof(struct prpsinfo *));
	oldbase = (struct oldproc *)
		malloc (oldprocs * sizeof(struct oldproc));

	ptable_size = nproc;

	if (pbase == NULL || pref == NULL || oldbase == NULL) {
		(void) fprintf(stderr, "%s: malloc: out of memory\n", myname);
		exit (1);
	}
}

void open_kmem()
{
#ifdef	PREKUDZU
	/*
	 * Opening KMEM is the ONLY part of top
	 * that requires setgid sys on IRIX so we do it early
	 * and then drop the privilege
	 */
	/* fprintf(stderr, "gid==%d, egid==%d\n", getgid(), getegid()); */
	if ((kmem = open(KMEM, O_RDONLY)) == -1) {
		fprintf(stderr, "top: can't open %s: %s\n",
			 KMEM, strerror(errno));
		exit(1);
	}
	/* Give up all privilege, irrevocably, from now on */
	if (setgid(getgid()) == -1) {
		fprintf(stderr, "top: can't reset gid: %s\n", strerror(errno));
		exit(1);
	}
	/* fprintf(stderr, "gid==%d, egid==%d\n", getgid(), getegid()); */
#else	/* PREKUDZU */
	return;
#endif	/* PREKUDZU */
}

char *start_cwd;
char **saved_argv;

int
machine_init(struct statics *statics)
{
	struct oldproc	*op, *endbase;
	int		pcnt = 0;
	struct utsname	utsname;
	char		tmpbuf[20];

	uname(&utsname);
	irix_ver = (float) atof((const char *)utsname.release);
	strncpy(tmpbuf, utsname.release, 9);
	tmpbuf[9] = '\0';
	sprintf(uname_str, "%s %-.14s %s %s",
		utsname.sysname, utsname.nodename,
		tmpbuf, utsname.machine);

	pagesize = getpagesize();

	if ((start_cwd = getcwd(NULL, -1)) == NULL) {
		(void) fprintf(stderr, "%s: Unable to getcwd\n", myname);
		return -1;
	}

	if (chdir(_PATH_PROCFSPI)) {
		/* handy for later on when we're reading it */
		(void) fprintf(stderr, "%s: Unable to chdir to %s\n",
					myname, _PATH_PROCFSPI);
		return -1;
	}
	if ((procdir = opendir(".")) == NULL) {
		(void) fprintf(stderr, "%s: Unable to open %s\n",
					myname, _PATH_PROCFSPI);
		return -1;
	}

	if ((avenrun_offset = sysmp(MP_KERNADDR, MPKA_AVENRUN)) == -1) {
		perror("sysmp(MP_KERNADDR, MPKA_AVENRUN)");
		return -1;
	}

	allocate_proc_tables();

	oldprocs = 2 * nproc;
	endbase = oldbase + oldprocs;
	for (op = oldbase; op < endbase; op++) {
		op->oldpid = -1;
	}

	statics->cpustate_names = cpustatenames;
	statics->memory_names = memorynames;
	statics->order_names = ordernames;
	statics->procstate_names = procstatenames;

	return (0);
}

void restart_top()
{
	int	fd;

	chdir(start_cwd);

	/* close all open files (we have very few so don't bother
	   with getrlimits/sysnconf etc.) except stdin/stdout/stderr */
	for (fd = 20; fd > fileno(stderr); fd--) {
		close(fd);
	}

        execvp(saved_argv[0], saved_argv);
	/* NOTREACHED */
}


char   *
format_header(uname_field)
	register char *uname_field;
{
	register char *ptr;

#ifndef	PREKUDZU
	if (do_threads > 1) {
		memcpy(header + THREAD_START, " THD", 4);
	}
#endif	/* PREKUDZU */
	ptr = header + UNAME_START;
	while (*uname_field != '\0') {
		*ptr++ = *uname_field++;
	}

	return (header);
}

void
get_system_info(si)
	struct system_info *si;
{
	int		i;
	int		avenrun[3];
	struct rminfo	realmem;
	struct sysinfo	sysinfo;
	static time_t	cp_old [CPU_STATES];
	static time_t	cp_diff[CPU_STATES];	/* for cpu state percentages */
	off_t		fswap;		/* current free swap in blocks */
	off_t		tswap;		/* total swap in blocks */

#ifdef	PREKUDZU
	(void) getkval(avenrun_offset, (int *) avenrun, sizeof(avenrun), "avenrun");
#else	/* PREKUDZU */
{
	sgt_cookie_t cookie;

	SGT_COOKIE_INIT(&cookie);
	SGT_COOKIE_SET_KSYM(&cookie, "avenrun");

	if (sysget(SGT_KSYM, (char *)avenrun, sizeof(avenrun),
		   SGT_READ, &cookie) != sizeof(avenrun)) {
		perror("sysget(avenrun)");
		exit(0);
	}
}
#endif	/* PREKUDZU */

	for (i = 0; i < 3; i++) {
		si->load_avg[i] = loaddouble(avenrun[i]);
		si->load_avg[i] /= 1024.0;
	}

	if ((numcpus = sysmp(MP_NPROCS)) == -1) {
		perror("sysmp(MP_NPROCS)");
		return;
	}

	if (sysmp(MP_SAGET, MPSA_RMINFO, &realmem, sizeof(realmem)) == -1) {
		perror("sysmp(MP_SAGET,MPSA_RMINFO, ...)");
		return;
	}

	swapctl(SC_GETFREESWAP, &fswap);
	swapctl(SC_GETSWAPTOT, &tswap);

	memory_stats[0] = pagetok(realmem.physmem);
	memory_stats[1] = pagetok(realmem.availrmem);
	memory_stats[2] = pagetok(realmem.freemem);
	memory_stats[3] = tswap / 2;
	memory_stats[4] = fswap / 2;

	if (sysmp(MP_SAGET,MPSA_SINFO, &sysinfo,sizeof(struct sysinfo)) == -1) {
		perror("sysmp(MP_SAGET,MPSA_SINFO)");
		return;
	}
	(void) percentages(CPU_STATES, cpu_states, sysinfo.cpu, cp_old, cp_diff);

	si->cpustates = cpu_states;
	si->memory = memory_stats;
	si->last_pid = -1;

	return;
}

caddr_t
get_process_info(si, sel, compare)
	struct system_info *si;
	struct process_select *sel;
	int     (*compare) ();

{
	int		i, total_procs, active_procs;
	struct prpsinfo	**prefp;
	struct prpsinfo	*pp;
	int		show_uid;
	static char	first_screen = 1;

	/* read all the proc structures */
	getptable(pbase);

	/* get a pointer to the states summary array */
	si->procstates = process_states;

	/* set up flags which define what we are going to select */
	show_uid = sel->uid != -1;

	/* count up process states and get pointers to interesting procs */
	total_procs = 0;
	active_procs = 0;
	(void) memset(process_states, 0, sizeof(process_states));
	prefp = pref;

	for (pp = pbase, i = 0; i < nproc; pp++, i++) {
		/*
		 * Place pointers to each valid proc structure in pref[].
		 * Process slots that are actually in use have a non-zero
		 * status field.  Processes with SSYS set are system
		 * processes---these get ignored unless show_system is set.
		 * Ariel: IRIX 6.4 had to redefine "system processes"
		 * They do not exist outside the kernel in new kernels.
		 * Now defining as uid==0 and ppid==1 (init children)
		 */
		if (pp->pr_state &&
			(sel->system || !(pp->pr_uid==0 && pp->pr_ppid==1))) {
			total_procs++;
			process_states[proc_state(pp)]++;
			/*
			 * zombies are actually interesting (to avoid)
			 * although they are not active, so I leave them
			 * displayed.
			 */
			if (/* (! pp->pr_zomb) && */
			    (sel->idle || IS_ACTIVE(pp)) &&
			    (! show_uid || pp->pr_uid == (uid_t) sel->uid)) {
				*prefp++ = pp;
				active_procs++;
			}
		}
	}
	first_screen = 0;

	/* if requested, sort the "interesting" processes */
	if (compare != NULL)
		qsort((char *) pref, active_procs, sizeof(struct prpsinfo *), compare);

	/* remember active and total counts */
	si->p_total = total_procs;
	si->p_active = active_procs;

	/* pass back a handle */
	handle.next_proc = pref;
	handle.remaining = active_procs;
	return ((caddr_t) &handle);
}

/*
 * Added cpu_id to running processes, add 'ready' (to run) state
 */
static char *
format_state(struct prpsinfo *pp)
{
	static char	state_buf[8];
	int		cpuid, state = proc_state(pp);
	
	if (state == S_RUNNING) {
		/*
		 * WARNING: this is not binary compatible with 6.2 !!!
		 * pp->pr_sonproc apparently had a different offset
		 * in 6.2.  Compiling with a 6.2 ROOT breaks this:
		 * we always get 0 in the pr_sonproc field.
		 *
		 * CPU-id: Support big machines (4096 CPUs)
		 * (4 digit pp->pr_sonproc) but also prevent wasting
		 * real estate on small machines: if cpuid is big
		 * lose chars from the state rather than making
		 * the field bigger.
		 */
		cpuid = pp->pr_sonproc;
		if (cpuid < 100) {
		    sprintf(state_buf, "run/%d", cpuid);
		} else if (cpuid < 1000) {
		    sprintf(state_buf, "ru/%d", cpuid);
		} else {
		    sprintf(state_buf, "r/%d", cpuid);
		}
		return state_buf;
	}

	/* default */
	return state_str[state];
}

static char *
format_prio(pp)
	struct prpsinfo *pp;
{
    static char	prio_str[10];

    if (irix_ver < 6.4) {
	/*
	 * Note: this is _compiled_ on 6.x where x >= 4 but I would like
	 * it to run on 6.2 6.3 as well (backward binary compatibility).
	 * Scheduling is completely different between these IRIX versions
	 * and some scheduling classes may even have different names.
	 *
	 * The solution: have more than one style of 'priority' depending
	 * on the OS version.
	 *
	 * See npri(1) + nice(2) + realtime(5) for scheduling classes,
	 * and priority values.
	 */
	if (pp->pr_pri <= NDPHIMIN)			/* real time? */
		sprintf(prio_str, "+%d", pp->pr_pri);
	else if (pp->pr_pri <= NDPNORMMIN)		/* normal interactive */
		sprintf(prio_str, "%d", pp->pr_pri);
	else						/* batch: low prio */
		sprintf(prio_str, "b%d", pp->pr_pri);

    } else {

	/* copied from Kostadis' old top code */

	if (strcmp(pp->pr_clname, "RT") == 0)		/* real time */
		sprintf(prio_str, "+%d", pp->pr_pri);
	else if (strcmp(pp->pr_clname, "DL") == 0)	/* unsupported ? */
		sprintf(prio_str, "d%d", pp->pr_pri);
	else if (strcmp(pp->pr_clname, "GN") == 0)
		sprintf(prio_str, "g%d", pp->pr_pri);
	else if (strcmp(pp->pr_clname, "GB") == 0)
		sprintf(prio_str, "p%d", pp->pr_pri);

	else if (strcmp(pp->pr_clname, "WL") == 0)	/* weightless */
		return "w";
	else if (strcmp(pp->pr_clname, "BC") == 0)
		return "bc";				/* batch critical */
	else if (strcmp(pp->pr_clname, "B") == 0)
		return "b";				/* batch */
	else
		sprintf(prio_str, "%d", pp->pr_pri);
    }
    return prio_str;
}

/*
 * Note: multithreaded processes on multiprocessor machines
 * may have more than 100% CPU utilization
 * (running on more than one CPU at the same instant)
 */
static char *
format_cpu(double pct)
{
    static char	buf[6];

    if (pct <= 0) {
	strcpy(buf, " 0.00");
    } else if (pct < 100) {
	sprintf(buf, "%5.2f", pct);		/* DD.DD% */
    } else if (pct < 1000) {
	sprintf(buf, "%5.1f", pct);		/* lose accuracy */
    } else if (pct < 10000) {
	sprintf(buf, "%5d", (long) pct);	/* lose all fraction */
    } else {
	strcpy(buf, ">9999");		/* too big to display accurately */
    }
    return buf;
}

/*
 * Like the above but save screen real estate:
 * Use only one digit after decimal point for the weighted average CPU %
 */
static char *
format_wcpu(double pct)
{
    static char	buf[6];

    if (pct <= 0) {
	strcpy(buf, " 0.0");
    } else if (pct < 100) {
	sprintf(buf, "%4.1f", pct);
    } else if (pct < 1000) {
	sprintf(buf, "%4d", (long) pct);
    } else {
	strcpy(buf, ">999");
    }
    return buf;
}

char *
format_next_process(handle, get_userid)
	caddr_t handle;
	char *(*get_userid)();
{
	struct prpsinfo	*pp;
	struct handle	*hp;
	long		cputime;

	/* find and remember the next proc structure */
	hp = (struct handle *) handle;
	pp = *(hp->next_proc++);
	hp->remaining--;

	/* get the process cpu usage since startup */
	cputime = pp->pr_time.tv_sec;

	/* format this entry */
	sprintf(fmt,
		Proc_format,
		pp->pr_pid,
#ifdef	PREKUDZU
		pp->pr_pgrp,
#else	/* PREKUDZU */
		(do_threads <= 1) ? pp->pr_pgrp : pp->pr_thds,
#endif	/* PREKUDZU */
		(*get_userid) (pp->pr_uid),
		format_prio(pp),
		format_k(pagetok(pp->pr_size)),
		format_k(pagetok(pp->pr_rssize)),
		format_state(pp),
		format_time(cputime),
		format_wcpu(weighted_cpu(pp)),
		format_cpu(percent_cpu(pp)),
		pp->pr_fname);

	/* return the result */
	return (fmt);
}

#ifdef	PREKUDZU
/*
 *  getkval(offset, ptr, size, refstr) - get a value out of the kernel.
 *	"offset" is the byte offset into the kernel for the desired value,
 *  	"ptr" points to a buffer into which the value is retrieved,
 *  	"size" is the size of the buffer (and the object to retrieve),
 *  	"refstr" is a reference string used when printing error meessages,
 *	    if "refstr" starts with a '!', then a failure on read will not
 *  	    be fatal (this may seem like a silly way to do things, but I
 *  	    really didn't want the overhead of another argument).
 *
 */

static int
getkval(offset, ptr, size, refstr)
	unsigned long offset;
	int    *ptr;
	int     size;
	char   *refstr;

{
	if (lseek(kmem, (long) offset, SEEK_SET) == -1) {
		if (*refstr == '!')
			refstr++;
		(void) fprintf(stderr, "%s: %s: lseek to %s: %s\n",
				myname, KMEM, refstr, strerror(errno));
		exit(0);
	}
	if (read(kmem, (char *) ptr, size) == -1) {
		if (*refstr == '!')
			return (0);
		else {
			(void) fprintf(stderr, "%s: %s: reading %s: %s\n",
				myname, KMEM, refstr, strerror(errno));
			exit(0);
		}
	}
	return (1);
}
#endif	/* PREKUDZU */

/*
 *  compare_K - comparison functions for "qsort"
 *	Compares the resource consumption of two processes using five
 *  	distinct keys.  The keys are:
 *  	percent cpu, cpu ticks, state, resident set size, total virtual
 *  	memory usage.  The process states are ordered as follows (from least
 *  	to most important):  WAIT, zombie, sleep, stop, idle, run.
 *  	Different comparison functions are used for different orderings.
 */

/* these are names given to allowed sorting orders -- first is default */
char *ordernames[] = {
	/*
	 * Aliases for user convenience/friendliness:
	 *	mem == size
	 *	rss == res
	 */
	"cpu", "size", "mem", "res", "rss",
	"time", "state", "command", "prio", NULL
};

/* forward definitions for comparison functions */
int compare_cpu(struct prpsinfo **pp1, struct prpsinfo **pp2);
int compare_size(struct prpsinfo **pp1, struct prpsinfo **pp2);
int compare_res(struct prpsinfo **pp1, struct prpsinfo **pp2);
int compare_time(struct prpsinfo **pp1, struct prpsinfo **pp2);
int compare_state(struct prpsinfo **pp1, struct prpsinfo **pp2);
int compare_cmd(struct prpsinfo **pp1, struct prpsinfo **pp2);
int compare_prio(struct prpsinfo **pp1, struct prpsinfo **pp2);

int (*proc_compares[])() = {
	compare_cpu,
	compare_size,
	compare_size,
	compare_res,
	compare_res,
	compare_time,
	compare_state,
	compare_cmd,
	compare_prio,
	NULL
};


/*
 * The possible comparison expressions.  These are defined in such a way
 * that they can be merely listed in the source code to define the actual
 * desired ordering.
 */

#define ORDERKEY_PCTCPU	\
	if (dresult = percent_cpu(p2) - percent_cpu(p1),\
	(result = dresult > 0.0 ? 1 : dresult < 0.0 ? -1 : 0) == 0)
#define ORDERKEY_CPTICKS \
	if ((result = p2->pr_time.tv_sec - p1->pr_time.tv_sec) == 0)
#define ORDERKEY_STATE  if ((result = proc_state(p2) - proc_state(p1)) == 0)
#define ORDERKEY_PRIO	if ((result = p2->pr_oldpri - p1->pr_oldpri) == 0)
#define ORDERKEY_RSSIZE	if ((result = p2->pr_rssize - p1->pr_rssize) == 0)
#define ORDERKEY_MEM	if ((result = (p2->pr_size - p1->pr_size)) == 0)
#define ORDERKEY_CMD	if ((result = strcmp(p1->pr_fname,p2->pr_fname)) == 0)

static int
compare_cpu(struct prpsinfo **pp1, struct prpsinfo **pp2)
{
	struct prpsinfo	*p1, *p2;
	int		result;
	double		dresult;

	/* remove one level of indirection */
	p1 = *pp1;
	p2 = *pp2;
	/*
	 * order by various keys, resorting to the next one
	 * whenever there's a tie in comparisons
	 */
	ORDERKEY_PCTCPU
	ORDERKEY_CPTICKS
	ORDERKEY_STATE
	ORDERKEY_PRIO
	ORDERKEY_RSSIZE
	ORDERKEY_MEM
	;
	return (result);
}

static int
compare_size(struct prpsinfo **pp1, struct prpsinfo **pp2)
{
	struct prpsinfo	*p1, *p2;
	int		result;
	double		dresult;

	/* remove one level of indirection */
	p1 = *pp1;
	p2 = *pp2;
	/*
	 * order by various keys, resorting to the next one
	 * whenever there's a tie in comparisons
	 */
	ORDERKEY_MEM
	ORDERKEY_RSSIZE
	ORDERKEY_PCTCPU
	ORDERKEY_CPTICKS
	ORDERKEY_STATE
	ORDERKEY_PRIO
	;
	return (result);
}

static int
compare_res(struct prpsinfo **pp1, struct prpsinfo **pp2)
{
	struct prpsinfo	*p1, *p2;
	int		result;
	double		dresult;

	/* remove one level of indirection */
	p1 = *pp1;
	p2 = *pp2;
	/*
	 * order by various keys, resorting to the next one
	 * whenever there's a tie in comparisons
	 */
	ORDERKEY_RSSIZE
	ORDERKEY_MEM
	ORDERKEY_PCTCPU
	ORDERKEY_CPTICKS
	ORDERKEY_STATE
	ORDERKEY_PRIO
	;
	return (result);
}

static int
compare_time(struct prpsinfo **pp1, struct prpsinfo **pp2)
{
	struct prpsinfo	*p1, *p2;
	int		result;
	double		dresult;

	/* remove one level of indirection */
	p1 = *pp1;
	p2 = *pp2;
	/*
	 * order by various keys, resorting to the next one
	 * whenever there's a tie in comparisons
	 */
	ORDERKEY_CPTICKS
	ORDERKEY_RSSIZE
	ORDERKEY_MEM
	ORDERKEY_PCTCPU
	ORDERKEY_STATE
	ORDERKEY_PRIO
	;
	return (result);
}

static int
compare_cmd(struct prpsinfo **pp1, struct prpsinfo **pp2)
{
	struct prpsinfo	*p1, *p2;
	int		result;
	double		dresult;

	/* remove one level of indirection */
	p1 = *pp1;
	p2 = *pp2;
	/*
	 * order by various keys, resorting to the next one
	 * whenever there's a tie in comparisons
	 */
	ORDERKEY_CMD
	ORDERKEY_PCTCPU
	ORDERKEY_CPTICKS
	ORDERKEY_RSSIZE
	;
	return (result);
}

static int
compare_state(struct prpsinfo **pp1, struct prpsinfo **pp2)
{
	struct prpsinfo	*p1, *p2;
	int		result;
	double		dresult;

	/* remove one level of indirection */
	p1 = *pp1;
	p2 = *pp2;
	/*
	 * order by various keys, resorting to the next one
	 * whenever there's a tie in comparisons
	 */
	ORDERKEY_STATE
	ORDERKEY_PCTCPU
	ORDERKEY_CPTICKS
	ORDERKEY_RSSIZE
	;
	return (result);
}

static int
compare_prio(struct prpsinfo **pp1, struct prpsinfo **pp2)
{
	struct prpsinfo	*p1, *p2;
	int		result;
	double		dresult;

	/* remove one level of indirection */
	p1 = *pp1;
	p2 = *pp2;
	/*
	 * order by various keys, resorting to the next one
	 * whenever there's a tie in comparisons
	 */
	ORDERKEY_PRIO
	ORDERKEY_PCTCPU
	;
	return (result);
}



/* return the owner of the specified process. */
uid_t
proc_owner(pid)
	pid_t   pid;
{
	register struct prpsinfo *p;
	int     i;

	for (i = 0, p = pbase; i < nproc; i++, p++)
		if (p->pr_pid == pid)
			return (p->pr_uid);

	return (-1);
}

#ifdef DO_MAPSIZE
static void
size(int fd, struct prpsinfo *ps)
{
	prmap_sgi_arg_t maparg;
	struct prmap_sgi maps[256];
	int	nmaps;
	double	sz;
	int	i;

	maparg.pr_vaddr = (caddr_t) maps;
	maparg.pr_size = sizeof maps;
	if ((nmaps = ioctl(fd, PIOCMAP_SGI, &maparg)) == -1) {
		/* XXX - this will be confusing */
		return;
	}
	for (i = 0, sz = 0; i < nmaps; ++i) {
		sz += (double) maps[i].pr_wsize / MA_WSIZE_FRAC;
	}
	ps->pr_rssize = (long) sz;
}
#endif

static double		timediff, alpha, beta;

static void compute_proc(int, struct oldproc *, struct prpsinfo *);
static int thread_iterate(int, int, struct oldproc *, struct prpsinfo *);

/* get process table */
void
getptable(struct prpsinfo *baseptr)
{
	struct prpsinfo		*currproc; /* ptr to current proc struct */
	int			i, numprocs;
	int			threads;
	struct dirent		*direntp;
	struct oldproc		*op, *endbase;
	struct timeval		thistime;
	static struct timeval	lasttime;

	/* measure time between last call to getptable and current call */
	gettimeofday (&thistime, NULL);

	/*
	 * To avoid divides, we keep times in nanoseconds.  This is
	 * scaled by 1e7 rather than 1e9 so that when we divide we
	 * get percent.
	 */
	timediff = ((double) thistime.tv_sec  * 1.0e7 -
		    (double) lasttime.tv_sec  * 1.0e7)
				+
		   ((double) thistime.tv_usec * 10 -
		    (double) lasttime.tv_usec * 10);

	/*
	 * Under extreme load conditions, sca has experienced
	 * an assert(timediff > 0) failure here. His guess is that
	 * sometimes timed resets the time backwards and gettimeofday
	 * returns a lower number on a later call.
	 * To be on the safe side I fix it here by setting timediff
	 * to some arbitrary small value (in nanoseconds).
	 */
	if (timediff <= 0.0) timediff = 100.0;

	lasttime = thistime;	/* prepare for next round */

	/*
	 * constants for exponential decaying average.
	 *	avg = alpha * new + beta * avg
	 * The goal is 50% decay in 30 sec.  However if the sample period
	 * is greater than 30 sec, there's not a lot we can do.
	 */
	if (timediff < 30.0e7) {
		alpha = 0.5 * (timediff / 15.0e7);
		beta = 1.0 - alpha;
	} else {
		alpha = 0.5;
		beta = 0.5;
	}
	assert(alpha >= 0); assert(alpha <= 1);
	assert(beta >= 0); assert(beta <= 1);

	endbase = oldbase + oldprocs;
	currproc = baseptr;

	for (numprocs = 0, rewinddir(procdir); direntp = readdir(procdir);) {
		int     fd;

		if ((fd = open(direntp->d_name, O_RDONLY)) < 0)
			continue;

		currproc = baseptr + numprocs;

		if (ioctl(fd, PIOCPSINFO, currproc) < 0) {
			(void) close(fd);
			continue;
		}

#ifdef	PREKUDZU
		threads = 1;
#else
		threads = currproc->pr_thds;
#endif	/* PREKUDZU */

		/*
		 * Prealocated process table overflow
		 *
		 * Getting 'nproc' from nlist(/unix) is not worth the
		 * kernel dependency (broke in 6.4 when nproc no longer
		 * exists).  6.4 introduced "sysget" which has an
		 * undocumented v.v_proc but it doesn't work in pre-kudzu
		 * systems and is likely to break in the future.
		 *
		 * realloc wouldn't work either because we have pointers
		 * to the proc table so we cannot just move it around.
		 * mmap(/dev/zero) and extend may not work on all systems
		 * and is more complex than the solution herein.
		 * The soultion: if there's an overflow simply restart top
		 * using execvp.
		 */

		if (numprocs + threads > ptable_size) {
			/*
			 * Compromise alert:
			 * This forgets interactive toggles
			 */
			restart_top();
			/* NOTREACHED */
		}

		numprocs += thread_iterate(fd, threads, endbase, currproc);
		(void) close(fd);

	}
	nproc = numprocs;

	/*
	 * Save current CPU time for next time around
	 * For the moment recreate the hash table each time, as the code
	 * is easier that way.
	 */
	oldprocs = 2 * nproc;
	endbase = oldbase + oldprocs;

	for (op = oldbase; op < endbase; op++)
		op->oldpid = -1;

	for (i = 0, currproc = baseptr; i < nproc; i++, currproc++) {

		/* find an empty spot */
		op = oldbase + HASH (currproc->pr_pid);
		for (;;) {
			if (op->oldpid == -1)
				break;
			op++;
			if (op == endbase)
				op = oldbase;
        	}
		op->oldpid = currproc->pr_pid;
#ifndef	PREKUDZU
		op->oldtid = currproc->pr_thds;
#endif	/* PREKUDZU */
		op->oldtime = (currproc->pr_time.tv_sec * 1.0e9 +
				currproc->pr_time.tv_nsec);
		op->oldpct = weighted_cpu(currproc);
	}
}

/* Warning: non-portable code ahead: do not reuse.
 */
static int
thread_iterate(int fd, int t, struct oldproc *endbase, struct prpsinfo *curp)
{
#ifdef	PREKUDZU
	compute_proc(fd, endbase, curp);
	return (1);
#else
	prthreadctl_t	ptc;
	int		expected = t;

	curp->pr_thds = 0;

	if (!do_threads || t == 1)  {
		compute_proc(fd, endbase, curp);
		return (1);
	}

	ptc.pt_tid = 0;
	ptc.pt_flags = PTFD_GEQ | PTFS_ALL;
	ptc.pt_cmd = PIOCPSINFO;
	ptc.pt_data = (caddr_t)curp;

	if (ioctl(fd, PIOCTHREAD, &ptc) < 0) {
		return (0);
	}
	curp->pr_thds = ptc.pt_tid;	/* ick */
	compute_proc(fd, endbase, curp);
	ptc.pt_flags = PTFD_GTR | PTFS_ALL;
	t--;
	do {
		ptc.pt_data = (caddr_t)(++curp);
		if (ioctl(fd, PIOCTHREAD, &ptc) < 0) {
			break;
		}
		curp->pr_thds = ptc.pt_tid;	/* ick */
		compute_proc(fd, endbase, curp);
	} while (t--);
	return (expected - t);
#endif	/* PREKUDZU */
}

static void
compute_proc(int fd, struct oldproc *endbase, struct prpsinfo *currproc)
{
	struct oldproc *op = oldbase + HASH (currproc->pr_pid);

	/*
	 * SVR4 doesn't keep track of CPU% in the kernel,
	 * so we have to do our own.
	 * See if we've heard of this process before.
	 * If so, compute % based on CPU since last time.
	 */
	for (;;) {
		if (op->oldpid == -1) /* not there */
			break;
		if (op->oldpid == currproc->pr_pid
#ifndef	PREKUDZU
		    && op->oldtid == currproc->pr_thds
#endif	/* PREKUDZU */
		) {
			/* found old data */
			percent_cpu(currproc) =
				((currproc->pr_time.tv_sec * 1.0e9 +
				currproc->pr_time.tv_nsec)
				- op->oldtime) / timediff;

			weighted_cpu(currproc) =
				op->oldpct * beta +
				percent_cpu(currproc) * alpha;

			break;
		}
		op++;		/* try next entry in hash table */
		if (op == endbase)    /* table wrap around */
			op = oldbase;
	}

	/* Otherwise, it's new, so use all of its CPU time */
	if (op->oldpid == -1) {
		percent_cpu(currproc) =
			(currproc->pr_time.tv_sec * 1.0e9 +
			currproc->pr_time.tv_nsec) / timediff;

		weighted_cpu(currproc) = percent_cpu(currproc);
	}
#ifdef DO_MAPSIZE
	size(fd, currproc);
#endif
}
