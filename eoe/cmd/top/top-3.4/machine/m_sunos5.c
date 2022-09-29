/*
 * top - a top users display for Unix
 *
 * SYNOPSIS:  Any Sun running SunOS 5.x (Solaris 2.x)
 *
 * DESCRIPTION:
 * This is the machine-dependent module for SunOS 5.x (Solaris 2).
 * There is some support for MP architectures.
 * This makes top work on the following systems:
 *         SunOS 5.0 (not tested)
 *         SunOS 5.1
 *         SunOS 5.2
 *         SunOS 5.3
 *
 *     Tested on a SPARCclassic with SunOS 5.1, using gcc-2.3.3, and
 *     SPARCsystem 600 with SunOS 5.2, using Sun C
 *
 *     Note that this same file is used for SunOS 5.4, but only with
 *     SOLARIS24 defined.  Use the file "m_sunos54.c" to make that happen
 *     automagically.
 *
 * LIBS: -lelf -lkvm -lkstat
 *
 * CFLAGS: -DHAVE_GETOPT -DORDER
 *
 *
 * AUTHORS:      Torsten Kasch 		<torsten@techfak.uni-bielefeld.de>
 *               Robert Boucher		<boucher@sofkin.ca>
 * CONTRIBUTORS: Marc Cohen 		<marc@aai.com>
 *               Charles Hedrick 	<hedrick@geneva.rutgers.edu>
 *	         William L. Jones 	<jones@chpc>
 *               Petri Kutvonen         <kutvonen@cs.helsinki.fi>
 *	         Casper Dik             <casper@fwi.uva.nl>
 *               Tim Pugh               <tpugh@oce.orst.edu>
 */
#define _KMEMUSER
#include "top.h"
#include "machine.h"
#include "utils.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <nlist.h>
#include <string.h>
#include <kvm.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/vm.h>
#include <sys/var.h>
#include <sys/cpuvar.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/priocntl.h>
#include <sys/tspriocntl.h>
#include <sys/processor.h>
#include <vm/anon.h>
#include <math.h>
#define USE_KSTAT
#ifdef USE_KSTAT
#include <kstat.h>
#endif

#define UNIX "/dev/ksyms"
#define KMEM "/dev/kmem"
#define PROCFS "/proc"
#define CPUSTATES     5
#ifndef PRIO_MIN
#define PRIO_MIN	-20
#endif
#ifndef PRIO_MAX
#define PRIO_MAX	20
#endif

#ifndef FSCALE
#define FSHIFT  8		/* bits to right of fixed binary point */
#define FSCALE  (1<<FSHIFT)
#endif /* FSCALE */

#define loaddouble(la) ((double)(la) / FSCALE)
#define dbl_align(x)	(((unsigned long)(x)+(sizeof(double)-1)) & \
						~(sizeof(double)-1))
#ifdef SOLARIS24
    /*
     * snarfed from <sys/procfs.h>:
     * The following percent numbers are 16-bit binary
     * fractions [0 .. 1] with the binary point to the
     * right of the high-order bit (one == 0x8000)
     */
#define percent_cpu(pp) (((double)pp->pr_pctcpu)/0x8000*100)
#define weighted_cpu(pp) (*(double *)dbl_align(pp->pr_filler))
#else
#define percent_cpu(pp) (*(double *)dbl_align(&pp->pr_filler[0]))
#define weighted_cpu(pp) (*(double *)dbl_align(&pp->pr_filler[2]))
#endif

/* definitions for indices in the nlist array */
#define X_AVENRUN		 0
#define X_MPID			 1
#define X_CPU			 2
#define X_V			 3
#define X_NPROC			 4
#define X_ANONINFO		 5
#define X_FREEMEM		 6
#define X_MAXMEM		 7
#define X_AVAILRMEM		 8
#define X_SWAPFS_MINFREE	 9
#define X_NCPUS		   	10

static struct nlist nlst[] =
{
  {"avenrun"},			/* 0 */
  {"mpid"},			/* 1 */
  {"cpu"},			/* 2 */
  {"v"},			/* 3 */
  {"nproc"},			/* 4 */
  {"anoninfo"},			/* 5 */
  {"freemem"},			/* 6 */
  {"maxmem"},			/* 7 */
  {"availrmem"},		/* 8 */
  {"swapfs_minfree"},		/* 9 */
  {"ncpus"},			/* 10 */
  {0}
};

static unsigned long avenrun_offset;
static unsigned long mpid_offset;
#ifdef USE_KSTAT
static kstat_t **cpu_ks;
static cpu_stat_t *cpu_stat;
#else
static unsigned long *cpu_offset;
#endif
static unsigned long nproc_offset;
static unsigned long freemem_offset;
static unsigned long maxmem_offset;
static unsigned long availrmem_offset;
static unsigned long swapfs_minfree_offset;
static unsigned long anoninfo_offset;

/* get_process_info passes back a handle.  This is what it looks like: */
struct handle
  {
    struct prpsinfo **next_proc;/* points to next valid proc pointer */
    int remaining;		/* number of pointers remaining */
  };

/*
 * Structure for keeping track of CPU times from last time around
 * the program.  We keep these things in a hash table, which is
 * recreated at every cycle.
 */
struct oldproc
  {
    pid_t oldpid;
    double oldtime;
    double oldpct;
  };
int oldprocs;			/* size of table */
#define HASH(x) ((x << 1) % oldprocs)

/*
 * GCC assumes that all doubles are aligned.  Unfortunately it
 * doesn't round up the structure size to be a multiple of 8.
 * Thus we'll get a coredump when going through array.  The
 * following is a size rounded up to 8.
 */
#define PRPSINFOSIZE dbl_align(sizeof(struct prpsinfo))

/*
 *  These definitions control the format of the per-process area
 */
static char header[] =
"  PID X        PRI NICE  SIZE   RES STATE   TIME   WCPU    CPU COMMAND";
/* 0123456   -- field to fill in starts at header+6 */
#define UNAME_START 6

#define Proc_format \
        "%5d %-8.8s %3d %4d %5s %5s %-5s %6s %5.2f%% %5.2f%% %s"

/* process state names for the "STATE" column of the display */
/* the extra nulls in the string "run" are for adding a slash and
   the processor number when needed */
char *state_abbrev[] =
{"", "sleep", "run", "zombie", "stop", "start", "cpu", "swap"};

int process_states[8];
char *procstatenames[] =
{
  "", " sleeping, ", " running, ", " zombie, ", " stopped, ",
  " starting, ", " on cpu, ", " swapped, ",
  NULL
};

int cpu_states[CPUSTATES];
char *cpustatenames[] =
{"idle", "user", "kernel", "iowait", "swap", NULL};
#define CPUSTATE_IOWAIT 3
#define CPUSTATE_SWAP   4


/* these are for detailing the memory statistics */
int memory_stats[5];
char *memorynames[] =
{"K real, ", "K active, ", "K free, ", "K swap, ", "K free swap", NULL};

/* these are names given to allowed sorting orders -- first is default */
char *ordernames[] = 
{"cpu", "size", "res", "time", NULL};

/* forward definitions for comparison functions */
int compare_cpu();
int compare_size();
int compare_res();
int compare_time();

int (*proc_compares[])() = {
    compare_cpu,
    compare_size,
    compare_res,
    compare_time,
    NULL };

kvm_t *kd;
static DIR *procdir;
static int nproc;
static int ncpus;

/* these are for keeping track of the proc array */
static int bytes;
static struct prpsinfo *pbase;
static struct prpsinfo **pref;
static struct oldproc *oldbase;

/* pagetok function is really a pointer to an appropriate function */
static int pageshift;
static int (*p_pagetok) ();
#define pagetok(size) ((*p_pagetok)(size))

/* useful externals */
extern int errno;
extern char *sys_errlist[];
extern char *myname;
extern int check_nlist ();
extern int gettimeofday ();
extern int getkval ();
extern void perror ();
extern void getptable ();
extern void quit ();
extern int nlist ();

int pagetok_none(int size)

{
    return(size);
}

int pagetok_left(int size)

{
    return(size << pageshift);
}

int pagetok_right(int size)

{
    return(size >> pageshift);
}

int
machine_init (struct statics *statics)
{
    static struct var v;
    struct oldproc *op, *endbase;
    int i;
    int offset;
    processor_info_t pi;

    /* perform the kvm_open */
    kd = kvm_open (NULL, NULL, NULL, O_RDONLY, "top");

    /* turn off super user privs */
    seteuid(getuid());

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;
    statics->order_names = ordernames;

    /* test kvm_open return value */
    if (kd == NULL)
      {
	perror ("kvm_open");
	return (-1);
      }
    if (kvm_nlist (kd, nlst) < 0)
      {
	perror ("kvm_nlist");
	return (-1);
      }
    if (check_nlist (nlst) != 0)
      return (-1);

    /* NPROC Tuning parameter for max number of processes */
    (void) getkval (nlst[X_V].n_value, &v, sizeof (struct var), nlst[X_V].n_name);
    nproc = v.v_proc;

    /* stash away certain offsets for later use */
    mpid_offset = nlst[X_MPID].n_value;
    nproc_offset = nlst[X_NPROC].n_value;
    avenrun_offset = nlst[X_AVENRUN].n_value;
    anoninfo_offset = nlst[X_ANONINFO].n_value;
    freemem_offset = nlst[X_FREEMEM].n_value;
    maxmem_offset = nlst[X_MAXMEM].n_value;
    availrmem_offset = nlst[X_AVAILRMEM].n_value;
    swapfs_minfree_offset = nlst[X_SWAPFS_MINFREE].n_value;

    (void) getkval (nlst[X_NCPUS].n_value, (int *) (&ncpus),
		    sizeof (ncpus), "ncpus");

#ifdef USE_KSTAT
    cpu_ks = (kstat_t **) malloc (ncpus * sizeof (kstat_t *));
    cpu_stat = (cpu_stat_t *) malloc (ncpus * sizeof (cpu_stat_t));
#else
    cpu_offset = (unsigned long *) malloc (ncpus * sizeof (unsigned long));
    for (i = offset = 0; i < ncpus; offset += sizeof(unsigned long)) {
        (void) getkval (nlst[X_CPU].n_value + offset,
                        &cpu_offset[i], sizeof (unsigned long),
                        nlst[X_CPU].n_name );
        if (cpu_offset[i] != 0)
            i++;
    }
#endif

    /* calculate pageshift value */
    i = sysconf(_SC_PAGESIZE);
    pageshift = 0;
    while ((i >>= 1) > 0)
    {
	pageshift++;
    }

    /* calculate an amount to shift to K values */
    /* remember that log base 2 of 1024 is 10 (i.e.: 2^10 = 1024) */
    pageshift -= 10;

    /* now determine which pageshift function is appropriate for the 
       result (have to because x << y is undefined for y < 0) */
    if (pageshift > 0)
    {
	/* this is the most likely */
	p_pagetok = pagetok_left;
    }
    else if (pageshift == 0)
    {
	p_pagetok = pagetok_none;
    }
    else
    {
	p_pagetok = pagetok_right;
	pageshift = -pageshift;
    }

    /* allocate space for proc structure array and array of pointers */
    bytes = nproc * PRPSINFOSIZE;
    pbase = (struct prpsinfo *) malloc (bytes);
    pref = (struct prpsinfo **) malloc (nproc * sizeof (struct prpsinfo *));
    oldbase = (struct oldproc *) malloc (2 * nproc * sizeof (struct oldproc));


    /* Just in case ... */
    if (pbase == (struct prpsinfo *) NULL || pref == (struct prpsinfo **) NULL
	|| oldbase == (struct oldproc *) NULL)
      {
	fprintf (stderr, "%s: can't allocate sufficient memory\n", myname);
	return (-1);
      }

    oldprocs = 2 * nproc;
    endbase = oldbase + oldprocs;
    for (op = oldbase; op < endbase; op++)
      op->oldpid = -1;

    if (!(procdir = opendir (PROCFS)))
      {
	(void) fprintf (stderr, "Unable to open %s\n", PROCFS);
	return (-1);
      }

    if (chdir (PROCFS))
      {				/* handy for later on when we're reading it */
	(void) fprintf (stderr, "Unable to chdir to %s\n", PROCFS);
	return (-1);
      }

    /* all done! */
    return (0);
  }

char *
format_header (register char *uname_field)
{
  register char *ptr;

  ptr = header + UNAME_START;
  while (*uname_field != '\0')
    *ptr++ = *uname_field++;

  return (header);
}

#ifdef USE_KSTAT

#define UPDKCID(nk,ok) \
if (nk == -1) { \
  perror("kstat_read "); \
  exit(1); \
} \
if (nk != ok)\
  goto kcid_changed;

kupdate()
{
  kstat_t *ks;
  kid_t nkcid;
  int i;
  int changed = 0;
  static kstat_ctl_t *kc = NULL;
  int ncpu = 0;
  static kid_t kcid = 0;

/*
 * 0. kstat_open
 */

  if (!kc) {
    kc = kstat_open();
    if (!kc) {
      perror("kstat_open ");
      exit(1);
    }
    changed = 1;
    kcid = kc->kc_chain_id;
  }

  /* keep doing it until no more changes */
 kcid_changed:

/*
 * 1.  kstat_chain_update
 */

  nkcid = kstat_chain_update(kc);
  if (nkcid) {  /* UPDKCID will abort if nkcid is -1, so no need to check */
    changed = 1;
    kcid = nkcid;
  }
  UPDKCID(nkcid,0);

  if (changed) {

/*
 * 2. get data addresses
 */

    ncpu = 0;
    for (ks = kc->kc_chain; ks; 
	 ks = ks->ks_next) {

      if (strncmp(ks->ks_name, "cpu_stat", 8) == 0) {
	nkcid = kstat_read(kc, ks, NULL);
	UPDKCID(nkcid, kcid);  /* if kcid changed, pointer might be invalid */

	cpu_ks[ncpu] = ks;
	ncpu++;
	if (ncpu > ncpus) {
	  fprintf(stderr, "kstat finds % CPUs, should be %d\n", ncpu, ncpus);
	  exit(1);
	}
      }
    }
    if (ncpu != ncpus) {
      fprintf(stderr, "kstat finds % CPUs, should be %d\n", ncpu, ncpus);
      exit(1);
    }
    changed = 0;
  }


/*
 * 3. get data
 */

  for (i = 0; i < ncpus; i++) {
    nkcid = kstat_read(kc, cpu_ks[i], &cpu_stat[i]);
    UPDKCID(nkcid, kcid);  /* if kcid changed, pointer might be invalid */
  }

}

#endif /* USE_KSTAT */

void
get_system_info (struct system_info *si)
{
  long avenrun[3];
  struct cpu cpu;
  static int freemem;
  static int maxmem;
  static int availrmem;
  static int swapfs_minfree;
  struct anoninfo anoninfo;
  static long cp_time[CPUSTATES];
  static long cp_old[CPUSTATES];
  static long cp_diff[CPUSTATES];
  register int j, i;

  /* get the cp_time array */
  for (j = 0; j < CPUSTATES; j++)
    cp_time[j] = 0L;

#ifdef USE_KSTAT
  /* use kstat to upadte all processor information */
  kupdate();
  for (i = 0; i < ncpus; i++)
    {
      /* sum counters up to, but not including, wait state counter */
      for (j = 0; j < CPU_WAIT; j++)
	cp_time[j] += (long) cpu_stat[i].cpu_sysinfo.cpu[j];

      /* add in wait state breakdown counters */
      cp_time[CPUSTATE_IOWAIT] += (long) cpu_stat[i].cpu_sysinfo.wait[W_IO] +
                                  (long) cpu_stat[i].cpu_sysinfo.wait[W_PIO];
      cp_time[CPUSTATE_SWAP] += (long) cpu_stat[i].cpu_sysinfo.wait[W_SWAP];
    }

#else /* !USE_KSTAT */

  for (i = 0; i < ncpus; i++)
    if (cpu_offset[i] != 0)
    {
      /* get struct cpu for this processor */
      (void) getkval (cpu_offset[i], &cpu, sizeof (struct cpu), "cpu");

      /* sum counters up to, but not including, wait state counter */
      for (j = 0; j < CPU_WAIT; j++)
	cp_time[j] += (long) cpu.cpu_stat.cpu_sysinfo.cpu[j];

      /* add in wait state breakdown counters */
      cp_time[CPUSTATE_IOWAIT] += (long) cpu.cpu_stat.cpu_sysinfo.wait[W_IO] +
                                  (long) cpu.cpu_stat.cpu_sysinfo.wait[W_PIO];
      cp_time[CPUSTATE_SWAP] += (long) cpu.cpu_stat.cpu_sysinfo.wait[W_SWAP];
    }

#endif /* USE_KSTAT */

  /* convert cp_time counts to percentages */
  (void) percentages (CPUSTATES, cpu_states, cp_time, cp_old, cp_diff);

  /* get mpid -- process id of last process */
  (void) getkval (mpid_offset, &(si->last_pid), sizeof (si->last_pid), "mpid");

  /* get load average array */
  (void) getkval (avenrun_offset, (int *) avenrun, sizeof (avenrun), "avenrun");

  /* convert load averages to doubles */
  for (i = 0; i < 3; i++)
    si->load_avg[i] = loaddouble (avenrun[i]);

  /* get system wide main memory usage structure */
  (void) getkval (freemem_offset, (int *) (&freemem), sizeof (freemem), "freemem");
  (void) getkval (maxmem_offset, (int *) (&maxmem), sizeof (maxmem), "maxmem");
  memory_stats[0] = pagetok (maxmem);
  memory_stats[1] = 0;
  memory_stats[2] = pagetok (freemem);
  (void) getkval (anoninfo_offset, (int *) (&anoninfo), sizeof (anoninfo), "anoninfo");
  (void) getkval (availrmem_offset, (int *) (&availrmem), sizeof (availrmem), "availrmem");
  (void) getkval (swapfs_minfree_offset, (int *) (&swapfs_minfree), sizeof (swapfs_minfree), "swapfs_minfree");
  memory_stats[3] = pagetok (anoninfo.ani_resv);
  memory_stats[4] = pagetok (MAX ((int) (anoninfo.ani_max - anoninfo.ani_resv), 0) + availrmem - swapfs_minfree);

  /* set arrays and strings */
  si->cpustates = cpu_states;
  si->memory = memory_stats;
}

static struct handle handle;

caddr_t
get_process_info (
		   struct system_info *si,
		   struct process_select *sel,
		   int (*compare) ())
{
  register int i;
  register int total_procs;
  register int active_procs;
  register struct prpsinfo **prefp;
  register struct prpsinfo *pp;

  /* these are copied out of sel for speed */
  int show_idle;
  int show_system;
  int show_uid;

  /* Get current number of processes */
  (void) getkval (nproc_offset, (int *) (&nproc), sizeof (nproc), "nproc");

  /* read all the proc structures */
  getptable (pbase);

  /* get a pointer to the states summary array */
  si->procstates = process_states;

  /* set up flags which define what we are going to select */
  show_idle = sel->idle;
  show_system = sel->system;
  show_uid = sel->uid != -1;

  /* count up process states and get pointers to interesting procs */
  total_procs = 0;
  active_procs = 0;
  (void) memset (process_states, 0, sizeof (process_states));
  prefp = pref;

  for (pp = pbase, i = 0; i < nproc;
       i++, pp = (struct prpsinfo *) ((char *) pp + PRPSINFOSIZE))
    {
      /*
	 *  Place pointers to each valid proc structure in pref[].
	 *  Process slots that are actually in use have a non-zero
	 *  status field.  Processes with SSYS set are system
	 *  processes---these get ignored unless show_sysprocs is set.
	 */
      if (pp->pr_state != 0 &&
	  (show_system || ((pp->pr_flag & SSYS) == 0)))
	{
	  total_procs++;
	  process_states[pp->pr_state]++;
	  if ((!pp->pr_zomb) &&
	      (show_idle || percent_cpu (pp) || (pp->pr_state == SRUN) || (pp->pr_state == SONPROC)) &&
	      (!show_uid || pp->pr_uid == (uid_t) sel->uid))
	    {
	      *prefp++ = pp;
	      active_procs++;
	    }
	}
    }

  /* if requested, sort the "interesting" processes */
  if (compare != NULL)
    qsort ((char *) pref, active_procs, sizeof (struct prpsinfo *), compare);

  /* remember active and total counts */
  si->p_total = total_procs;
  si->p_active = active_procs;

  /* pass back a handle */
  handle.next_proc = pref;
  handle.remaining = active_procs;
  return ((caddr_t) & handle);
}

char fmt[MAX_COLS];			/* static area where result is built */

char *
format_next_process (
		      caddr_t handle,
		      char *(*get_userid) ())
{
  register struct prpsinfo *pp;
  struct handle *hp;
  register long cputime;
  register double pctcpu;

  /* find and remember the next proc structure */
  hp = (struct handle *) handle;
  pp = *(hp->next_proc++);
  hp->remaining--;

  /* get the cpu usage and calculate the cpu percentages */
  cputime = pp->pr_time.tv_sec;
  pctcpu = percent_cpu (pp);

  /* format this entry */
  sprintf (fmt,
	   Proc_format,
	   pp->pr_pid,
	   (*get_userid) (pp->pr_uid),
	   pp->pr_pri - PZERO,
	   pp->pr_nice - NZERO,
	   format_k(pp->pr_bysize / 1024),
	   format_k(pp->pr_byrssize / 1024),
	   state_abbrev[pp->pr_state],
	   format_time(cputime),
	   weighted_cpu (pp),
	   pctcpu,
	   pp->pr_fname);

  /* return the result */
  return (fmt);
}

/*
 * check_nlist(nlst) - checks the nlist to see if any symbols were not
 *		found.  For every symbol that was not found, a one-line
 *		message is printed to stderr.  The routine returns the
 *		number of symbols NOT found.
 */
int
check_nlist (register struct nlist *nlst)
{
  register int i;

  /* check to see if we got ALL the symbols we requested */
  /* this will write one line to stderr for every symbol not found */

  i = 0;
  while (nlst->n_name != NULL)
    {
      if (nlst->n_type == 0)
	{
	  /* this one wasn't found */
	  fprintf (stderr, "kernel: no symbol named `%s'\n", nlst->n_name);
	  i = 1;
	}
      nlst++;
    }
  return (i);
}


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
int
getkval (unsigned long offset,
	 int *ptr,
	 int size,
	 char *refstr)
{
  if (kvm_read (kd, offset, (char *) ptr, size) != size)
    {
      if (*refstr == '!')
	{
	  return (0);
	}
      else
	{
	  fprintf (stderr, "top: kvm_read for %s: %s\n", refstr, sys_errlist[errno]);
	  quit (23);
	}
    }
  return (1);

}

/* comparison routines for qsort */

/*
 * There are currently four possible comparison routines.  main selects
 * one of these by indexing in to the array proc_compares.
 *
 * Possible keys are defined as macros below.  Currently these keys are
 * defined:  percent cpu, cpu ticks, process state, resident set size,
 * total virtual memory usage.  The process states are ordered as follows
 * (from least to most important):  WAIT, zombie, sleep, stop, start, run.
 * The array declaration below maps a process state index into a number
 * that reflects this ordering.
 */

/* First, the possible comparison keys.  These are defined in such a way
   that they can be merely listed in the source code to define the actual
   desired ordering.
 */

#define ORDERKEY_PCTCPU  if (dresult = percent_cpu (p2) - percent_cpu (p1),\
			     (result = dresult > 0.0 ? 1 : dresult < 0.0 ? -1 : 0) == 0)
#define ORDERKEY_CPTICKS if ((result = p2->pr_time.tv_sec - p1->pr_time.tv_sec) == 0)
#define ORDERKEY_STATE   if ((result = (long) (sorted_state[p2->pr_state] - \
			       sorted_state[p1->pr_state])) == 0)
#define ORDERKEY_PRIO    if ((result = p2->pr_oldpri - p1->pr_oldpri) == 0)
#define ORDERKEY_RSSIZE  if ((result = p2->pr_rssize - p1->pr_rssize) == 0)
#define ORDERKEY_MEM     if ((result = (p2->pr_size - p1->pr_size)) == 0)

/* Now the array that maps process state to a weight */

unsigned char sorted_state[] =
{
  0,				/* not used		*/
  3,				/* sleep		*/
  6,				/* run			*/
  2,				/* zombie		*/
  4,				/* stop			*/
  5,				/* start		*/
  7,				/* run on a processor   */
  1				/* being swapped (WAIT)	*/
};


/* compare_cpu - the comparison function for sorting by cpu percentage */

int
compare_cpu (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return (result);
  }

/* compare_size - the comparison function for sorting by total memory usage */

int
compare_size (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ;

    return (result);
  }

/* compare_res - the comparison function for sorting by resident set size */

int
compare_res (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ;

    return (result);
  }

/* compare_time - the comparison function for sorting by total cpu time */

int
compare_time (
	       struct prpsinfo **pp1,
	       struct prpsinfo **pp2)
  {
    register struct prpsinfo *p1;
    register struct prpsinfo *p2;
    register long result;
    double dresult;

    /* remove one level of indirection */
    p1 = *pp1;
    p2 = *pp2;

    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ;

    return (result);
  }

/*
get process table
 V.4 only has a linked list of processes so we want to follow that
 linked list, get all the process structures, and put them in our own
 table
*/
void
getptable (struct prpsinfo *baseptr)
{
  struct prpsinfo *currproc;	/* pointer to current proc structure	*/
  int numprocs = 0;
  int i;
  struct dirent *direntp;
  struct oldproc *op;
  static struct timeval lasttime =
  {0, 0};
  struct timeval thistime;
  double timediff;
  double alpha, beta;
  struct oldproc *endbase;

  gettimeofday (&thistime, NULL);
  /*
   * To avoid divides, we keep times in nanoseconds.  This is
   * scaled by 1e7 rather than 1e9 so that when we divide we
   * get percent.
   */
  if (lasttime.tv_sec)
    timediff = ((double) thistime.tv_sec * 1.0e7 +
		((double) thistime.tv_usec * 10.0)) -
      ((double) lasttime.tv_sec * 1.0e7 +
       ((double) lasttime.tv_usec * 10.0));
  else
    timediff = 1.0e7;

  /*
     * constants for exponential average.  avg = alpha * new + beta * avg
     * The goal is 50% decay in 30 sec.  However if the sample period
     * is greater than 30 sec, there's not a lot we can do.
     */
  if (timediff < 30.0e7)
    {
      alpha = 0.5 * (timediff / 30.0e7);
      beta = 1.0 - alpha;
    }
  else
    {
      alpha = 0.5;
      beta = 0.5;
    }

  endbase = oldbase + oldprocs;
  currproc = baseptr;

  /* before reading /proc files, turn on root privs */
  /* (we don't care if this fails since it will be caught later) */
  seteuid(0);

  for (rewinddir (procdir); (direntp = readdir (procdir));)
    {
      int fd;

      if ((fd = open (direntp->d_name, O_RDONLY)) < 0)
	continue;

      if (ioctl (fd, PIOCPSINFO, currproc) < 0)
	{
	  (void) close (fd);
	  continue;
	}

      /*
       * SVr4 doesn't keep track of CPU% in the kernel, so we have
       * to do our own.  See if we've heard of this process before.
       * If so, compute % based on CPU since last time.
       */
      op = oldbase + HASH (currproc->pr_pid);
      while (1)
	{
	  if (op->oldpid == -1)	/* not there */
	    break;
	  if (op->oldpid == currproc->pr_pid)
	    {			/* found old data */
#ifndef SOLARIS24
	      percent_cpu (currproc) =
		((currproc->pr_time.tv_sec * 1.0e9 +
		  currproc->pr_time.tv_nsec)
		 - op->oldtime) / timediff;
#endif
	      weighted_cpu (currproc) =
		op->oldpct * beta + percent_cpu (currproc) * alpha;

	      break;
	    }
	  op++;			/* try next entry in hash table */
	  if (op == endbase)	/* table wrapped around */
	    op = oldbase;
	}

      /* Otherwise, it's new, so use all of its CPU time */
      if (op->oldpid == -1)
	{
#ifdef SOLARIS24
	  weighted_cpu (currproc) =
	    percent_cpu (currproc);
#else
	  if (lasttime.tv_sec)
	    {
	      percent_cpu (currproc) =
		(currproc->pr_time.tv_sec * 1.0e9 +
		 currproc->pr_time.tv_nsec) / timediff;
	      weighted_cpu (currproc) =
		percent_cpu (currproc);
	    }
	  else
	    {			/* first screen -- no difference is possible */
	      percent_cpu (currproc) = 0.0;
	      weighted_cpu (currproc) = 0.0;
	    }
#endif
	}

      numprocs++;
      currproc = (struct prpsinfo *) ((char *) currproc + PRPSINFOSIZE);
      (void) close (fd);
    }

  /* turn off root privs */
  seteuid(getuid());

  if (nproc != numprocs)
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
  for (i = 0, currproc = baseptr;
       i < nproc;
     i++, currproc = (struct prpsinfo *) ((char *) currproc + PRPSINFOSIZE))
    {
      /* find an empty spot */
      op = oldbase + HASH (currproc->pr_pid);
      while (1)
	{
	  if (op->oldpid == -1)
	    break;
	  op++;
	  if (op == endbase)
	    op = oldbase;
	}
      op->oldpid = currproc->pr_pid;
      op->oldtime = (currproc->pr_time.tv_sec * 1.0e9 +
		     currproc->pr_time.tv_nsec);
      op->oldpct = weighted_cpu (currproc);
    }
  lasttime = thistime;
}

/*
 * proc_owner(pid) - returns the uid that owns process "pid", or -1 if
 *              the process does not exist.
 *              It is EXTREMLY IMPORTANT that this function work correctly.
 *              If top runs setuid root (as in SVR4), then this function
 *              is the only thing that stands in the way of a serious
 *              security problem.  It validates requests for the "kill"
 *              and "renice" commands.
 */
uid_t
proc_owner (pid_t pid)
{
  register struct prpsinfo *p;
  int i;
  for (i = 0, p = pbase; i < nproc;
       i++, p = (struct prpsinfo *) ((char *) p + PRPSINFOSIZE)) {
    if (p->pr_pid == pid)
      return (p->pr_uid);
  }
  return (-1);
}

int
setpriority (int dummy, int who, int niceval)
{
  int scale;
  int prio;
  pcinfo_t pcinfo;
  pcparms_t pcparms;
  tsparms_t *tsparms;

  strcpy (pcinfo.pc_clname, "TS");
  if (priocntl (0, 0, PC_GETCID, (caddr_t) & pcinfo) == -1)
    return (-1);

  prio = niceval;
  if (prio > PRIO_MAX)
    prio = PRIO_MAX;
  else if (prio < PRIO_MIN)
    prio = PRIO_MIN;

  tsparms = (tsparms_t *) pcparms.pc_clparms;
  scale = ((tsinfo_t *) pcinfo.pc_clinfo)->ts_maxupri;
  tsparms->ts_uprilim = tsparms->ts_upri = -(scale * prio) / 20;
  pcparms.pc_cid = pcinfo.pc_cid;

  if (priocntl (P_PID, who, PC_SETPARMS, (caddr_t) & pcparms) == -1)
    return (-1);

  return (0);
}
