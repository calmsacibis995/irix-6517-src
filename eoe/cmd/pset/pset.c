/*
 * pset - manage processor sets
 *
 * pset 
 *    -> display processor set activity
 *
 * pset -i
 *    -> initialize process sets from the description file, or the file named
 *       with the -f option
 *
 * pset -s {[name] {[set]|[+set]|[!set]}}
 *    -> display and manage processor sets
 *	[set]  - create a set 'name' with processers 'set'
 *	[+set] - add the processors in 'set' to set 'name'
 *	[-set] - delete the processors in 'set' from set 'name'
 *
 * pset -r name
 *    -> remove the processor set 'name'
 *
 * pset -p pid,pid... [name]
 *    -> display and manage process affilition with a set
 *	[name] - set the processor set for 'pid' to 'name'
 *
 * pset -u pid,pid...
 *    -> disassociate a process from any set
 *
 * pset -c name [cmd args ...]
 *    -> run the given command under the set 'name', shell by default
 *
 * pset -P set1 [set2]
 *    -> display and manage processes in a set
 *	[set2] - move all processes in set1 to set2
 *
 * pset -d {[disc]|[disc name]}
 *    -> display and manage the sets associated with disciplines
 *
 * All commands support the following additional options, specified before
 * the command letter:
 *
 * -f file	- use the file for finding processor set names and vectors
 *		  instead of the default
 * -v		- be a bit more verbose
 */

# define _KMEMUSER

# include	<sys/types.h>
# include	<sys/param.h>
/* # include	<sys/splock.h> */
# include	<sys/schedctl.h>
# include	<sys/prctl.h>
# include	<sys/time.h>
# include	<sys/timers.h>
#define __SYS_SYSTM_H__
# include	<sys/runq_private.h>
# include	<sys/signal.h>
# include	<sys/procfs.h>

# include	<dirent.h>
# include	<string.h>
# include	<fcntl.h>
# include 	<stdio.h>
# include	<stdarg.h>
# include	<stdlib.h>

# define	DEFFILE		"/etc/psettab"
# define	MAXQ		20
# define	MAXARG		500

/*
 * By default, sproc() would try to reserve space for stack with
 * size = u.u_rlimit[RLIMIT_STACK].rlim_cur. Some suid programs
 * change this limit to RLIM32_INFINITY before exec'ing pset,
 * with the result that sproc() tries to reserve space for a huge stack.
 * Therefore, use sprocsp() instead with the following stacksize.
 * The magic number for the size is derived from the default rlim_cur.
 * Used by readtab() in this file.
 */
#define 	STACKSIZE	(65536*1024)

extern int	errno;

char		*pname;
char		*mem = "/dev/kmem";
int		memfd;
DIR		*dd;
char		*path;
char		*dfile = DEFFILE;
FILE		*dffd = 0;
runq_t		Runq;
bvinfo_t	apset;
int		setsz = sizeof(bvinfo_t);
bvinfo_t	*sets = 0;
int		verbose = 0;
bvinfo_t	active;

ulong		tosname(char *);
char		*printset(bvinfo_t *);
char		*tosym(bvname_t);
void		getset(char *, sbv_t *);
int		symtovec(char *, sbv_t *);
char *		ntosym(ulong);
ulong		symton(char *);
bvinfo_t	*findset(ulong);

/*
 * Each top-level command is executed via it's command procedure. The
 * format of a command procedure is:
 *
 *	command(arg1, cnt, argv)
 *
 * Where:
 *   arg1 - is the argument to the command, or zero if none
 *   cnt  - the number of valid entries in argv, if needed
 *   argv - is an array of additional arguments, if any, to the command.
 */
void		showstatus();
int		opsets(bvname_t, int, char **);
int		delset(bvname_t, int, char **);
int		pidsets(int, int, char **);
int		upidsets(int, int, char **);
void		runset(bvname_t, int, char **);
int		masspid(bvname_t, int, char **);
void		kread(void *, void *, int);
void		readsets(void);
void		inittab(void);
void		readtab(void);

# define	SNAME		1
# define	LIST		2
# define	CHARP		3
# define	NONE		4

typedef int (*func_t)();
typedef struct {
	char	cmd;
	char	a1type;
	int	minarg;
	int	maxarg;
	func_t	func;
} cmd_t;
cmd_t cmds[] = {
	{ 's', SNAME, 0, 2, (func_t) opsets },
	{ 'r', SNAME, 1, MAXARG, (func_t) delset },
	{ 'p', LIST, 1, 2, (func_t) pidsets },
	{ 'u', LIST, 0, 1, (func_t) upidsets },
	{ 'P', SNAME, 0, 2, (func_t) masspid },
	{ 0 }
};
int	isnocmd = 1;
int	initcmd = 0;
pid_t	pidlist[MAXARG];
int	npids;

void		vperror(const char *fmt, ...);

main(argc, argv)
   int		argc;
   char		*argv[];
{
   int		ai;
   runq_t	*rqp;
   int		rqps;
   int		most;
   int		vec;
   int		i;

	pname = argv[0];

	if ((memfd = open(mem, O_RDONLY)) == -1) {
		vperror("%s: memory special file", pname);
		exit(1);
	}

	/*
	 * Get the runq.
	 */
	if ((rqp=(runq_t *) sysmp(MP_KERNADDR, MPKA_RUNQ)) == (runq_t *)(__psint_t) -1) {
		vperror("%s: find run queue", pname);
		exit(1);
	}
	if ((rqps = sysmp(MP_SASZ, MPSA_RUNQ)) == -1)
		rqps = sizeof(runq_t);
	else
		rqps = (rqps > sizeof(runq_t) ? sizeof(runq_t) : rqps);
	kread(rqp, &Runq, rqps);

	/*
	 * Get the active processors.
	 */
	if ((setsz = sysmp(MP_SASZ, MPSA_PSET)) == -1)
		setsz = sizeof(bvinfo_t);
	else
		setsz = (setsz > sizeof(bvinfo_t) ? sizeof(bvinfo_t) : setsz);
	kread(Runq._active_pset, &apset, setsz);
	Runq._active_pset = &apset;

	most = Runq._highest_active;
	for (i = 0; i <= most; i++)
		active.bv_vec |= ((sbv_t)1 << i);

	/*
	 * Access the process information.
	 */
	if ((dd = opendir("/proc")) == NULL) {
		if ((dd = opendir("/debug")) == NULL) {
			vperror("%s: open process directory", pname);
			exit(1);
		}
		else
			path = "/debug";
	}
	else
		path = "/proc";
	seteuid(getuid());

	if (parseargs(argc - 1, &argv[1]))
		exit(1);
	readtab();
	if (initcmd)
		inittab();
	else if (isnocmd)
		showstatus();
	exit(0);
}

char *
getnext(char *opt) {
	if (*opt == ' ') {
		while (*opt != '\0' && *opt == ' ') opt++;
		if (*opt == '\0')
			return(0);
		return(opt);
	}
	else if (*opt == '\0')
		return(0);
	else
		return(opt);
}

int
parseargs(int argc, char **argv)
{
   int		i;
   int		j;
   int		narg;
   int		idx;
   int		err = 0;
   cmd_t	*cp;
   char		*cargs[MAXARG];
   char		*adjacent;
   char		opt;

	i = 0;
	idx = 0;
	while (i < argc) {
		if (idx == 0 && argv[i][0] != '-') {
			err++;
			break;
		}
		opt = argv[i][++idx];
		if (opt == '\0') {
			/* done with this argument */
			i++;
			idx = 0;
			continue;
		}
		switch (opt) {
		case 'v':
			verbose++;
			continue;
		case 'i':
			initcmd = 1;
			continue;
		case 'f':
			if (!(dfile = getnext(&argv[i][idx + 1])))
				if (i == argc - 1)
					err++;
				else
					dfile = argv[++i];
			break;
		case 'c':
			isnocmd = 0;
			readtab();
			if (initcmd) {
				inittab();
				initcmd = 0;
			}

			narg = 0;
			while (narg < (MAXARG-1) && argc - i > 1)
				cargs[narg++] = argv[++i];
			cargs[narg] = NULL;
			if (narg < 1 || narg >= (MAXARG-1)) {
				err++;
				break;
			}

			runset(tosname(cargs[0]), narg, &cargs[1]);
			break;
		case 's':
		case 'd':
		case 'r':
		case 'p':
		case 'P':
		case 'u':
			isnocmd = 0;
			readtab();
			if (initcmd) {
				inittab();
				initcmd = 0;
			}
			for (cp = &cmds[0] ; cp->cmd != 0 ; cp++ ) {
				if (cp->cmd == argv[i][idx])
					break;
			}
			if (cp->cmd == 0) {
				fprintf(stderr, "%s: programmer error 1\n",
									pname);
				exit(1);
			}

			narg = 0;
			if ((adjacent = getnext(&argv[i][idx + 1])))
				cargs[narg++] = adjacent;
			while (narg < cp->maxarg && argc - i > 1) {
				if (argv[i + 1][0] == '-')
					break;
				cargs[narg++] = argv[++i];
			}
			if (narg < cp->minarg || narg > cp->maxarg) {
				err++;
				break;
			}

			switch (cp->a1type) {
			case SNAME:
				if (narg == 0)
					err += (*(cp->func))(0, 0, 0);
				else
					err += (*(cp->func))(tosname(cargs[0]),
							     narg,
							     &cargs[1]);
				break;
			case LIST:
				if (narg == 0)
					npids = 0;
				else {
				   char	*s;
				   char	*t = cargs[0];
				   auto char *j;
				   int	k = 0;

					if ((s = strtok(t, ",")) == NULL) {
						pidlist[k++] = strtoul(t, &j, 0);
						if (j == t) {
							fprintf(stderr, "%s: invalid number %s\n",
								pname, t);
							exit(1);
						}
					} else while (s != NULL) {
						pidlist[k++] = strtoul(s, &j, 0);
						if (j == s) {
							fprintf(stderr, "%s: invalid number %s\n",
								pname, s);
							exit(1);
						}
						s = strtok(NULL, ",");
					}
					npids = k;
				}
				err += (*(cp->func))(npids,
						     narg,
						     &cargs[1]);
				break;
			case NONE:
				err += (*(cp->func))();
				break;
			default:
				fprintf(stderr, "%s: programmer error 2\n",
									pname);
				exit(1);
			}
			break;
		default:
			err++;
			break;
		}
		i++;
		idx = 0;
	}
	if (err) {
		fprintf(stderr,
			"Display and manage processor set information:\n");
		fprintf(stderr, "usage: %s [options ...]\n", pname);
		fprintf(stderr,
			"\t-s [name]|[name {[procs]|[+procs]|[!procs]}]\n");
		fprintf(stderr, "\t    => manipulate sets\n");
		fprintf(stderr, "\t-r name\n");
		fprintf(stderr, "\t    => remove the given set\n");
		fprintf(stderr, "\t-d [disc]|[disc set]\n");
		fprintf(stderr, "\t    => manage discipline settings\n");
		fprintf(stderr, "\t-p pid[,pid,pid...] [set]\n");
		fprintf(stderr, "\t    => manage process settings\n");
		fprintf(stderr, "\t-u pid[,pid,pid...]\n");
		fprintf(stderr, "\t    => remove process settings\n");
		fprintf(stderr, "\t-P [set1 [set2]]\n");
		fprintf(stderr, "\t    => display or change process set\n");
		fprintf(stderr, "\t-c set [cmd args ...]\n");
		fprintf(stderr, "\t    => run a command under a set\n");
		fprintf(stderr, "\t-f file\n");
		fprintf(stderr, "\t    => take name information from file\n");
		fprintf(stderr, "\t-i\n");
		fprintf(stderr, "\t    => initialize system sets\n");
		fprintf(stderr, "\t-v\n");
		fprintf(stderr, "\t    => verbose mode\n");
		exit(1);
	}
	return(err);
}

/*
 * Operations on processor sets.
 */
# define	SFORMAT		"%-12.12s(%10d) %*s %-8s %d\n"
# define	SFORMATH	"%-12.12s %11s  %12s %-8s %s\n"

int
opsets(bvname_t sn, int narg, char **arg)
{
   bvinfo_t	*bp;
   bvinfo_t	tbp;
   char		*procs;
   int		op;
   sbv_t	tset;

	/*
	printf(SFORMATH,
		"Set_Name",
		"Set_ID",
		"Set_Vector",
		"Special",
		"Count");
	*/
	if (narg == 0) {
		/*
		 * List all sets.
		 */
		readsets();
		bp = sets;
		while (bp != 0) {
			if (!_bvfIssys(bp) || verbose) {
				printf(SFORMAT,
					tosym(bp->bv_name),
					bp->bv_name,
					((sizeof(sbv_t)*8)/4)+2,
					printset(bp),
					(_bvfIssys(bp) ? "sys" : ""),
					bp->bv_ref);
			}
			bp = bp->bv_next;
		}
		return(0);
	}
	if (narg == 1) {
		/*
		 * List one set.
		 */
		if (bp = findset(sn))
			printf(SFORMAT,
				tosym(bp->bv_name),
				bp->bv_name,
				((sizeof(sbv_t)*8)/4)+2,
				printset(bp),
				(_bvfIssys(bp) ? "sys" : ""),
				bp->bv_ref);
		return(0);
	}

	/*
	 * Add or modify sets.
	 */
	procs = arg[0];
	if (*procs == '!') {
		op = 2;
		procs++;
	}
	else if (*procs == '+') {
		op = 1;
		procs++;
	}
	else
		op = 0;
	if (procs[0] < '0' || procs[0] > '9') {
		if (!symtovec(procs, &tset)) {
			vperror("%s: no name '%s' known\n", pname, procs);
			return(1);
		}
	}
	else
		getset(procs, &tset);
	switch (op) {
	case 0:
		if (sysmp(MP_PSET, MPPS_CREATE, sn, &tset) == -1) {
			vperror("%s: create processor set %s", pname,
				tosym(sn));
			exit(1);
		}
		break;
	case 1:
		if (sysmp(MP_PSET, MPPS_ADD, sn, &tset) == -1) {
			vperror("%s: add to processor set %s", pname,
				tosym(sn));
			exit(1);
		}
		break;
	case 2:
		if (sysmp(MP_PSET, MPPS_REMOVE, sn, &tset) == -1) {
			vperror("%s: remove from processor set %s", pname,
				tosym(sn));
			exit(1);
		}
		break;
	}
	return(0);
}

/*
 * Delete a processor set.
 */
int
delset(bvname_t set, int narg, char **arg)
{
   int		i;
   bvname_t	nset;
   int		nerr = 0;

	if (sysmp(MP_PSET, MPPS_DELETE, set) == -1) {
		vperror("%s: delete set %s", pname, tosym(set));
		nerr++;
	}
	for (i = 1; i < narg; i++) {
		nset = tosname(arg[i-1]);
		if (sysmp(MP_PSET, MPPS_DELETE, nset) == -1) {
			vperror("%s: delete set %s", pname, tosym(nset));
			nerr++;
		}
	}
	if (nerr)
		exit(1);
	return(0);
}

/*
 * Operations on processes.
 */
int
pidsets(int npid, int narg, char **arg)
{
   char		path[BUFSIZ];
   int		fd;
   struct prpsinfo	psi;
   bvname_t	bvn;
   bvinfo_t	*lbp = 0;
   int		pcnt;
   int		ecnt = 0;

	if (npid == 0) {
		pidlist[0] = getppid();
		npid = 1;
	}
	for (pcnt = 0; pcnt < npid; pcnt++) {
		sprintf(path, "/proc/%d", pidlist[pcnt]);
		if ((fd = open(path, O_RDONLY)) == -1) {
			sprintf(path, "/debug/%d", pidlist[pcnt]);
			if ((fd = open(path, O_RDONLY)) == -1) {
				vperror("%s: accessing process %d", pname,
					pidlist[pcnt]);
				exit(1);
			}
		}
		if (narg == 2) {
			bvn = tosname(arg[0]);
			if (schedctl(SETPSET, pidlist[pcnt], bvn) == -1) {
				vperror("%s: assign process %d to set %s",
					pname, pidlist[pcnt], tosym(bvn));
				ecnt++;
				continue;
			}
		}
		if (ioctl(fd, PIOCPSINFO, &psi) == -1) {
			vperror("%s: accessing process %d", pname,
						pidlist[pcnt]);
			exit(1);
		}
		printf("pid %d in ", pidlist[pcnt]);
		if (lbp = findset(psi.pr_pset))
			printf("set %s(%s) ",
					tosym(lbp->bv_name),
					printset(lbp));
		else
			printf("no set ", pidlist[pcnt]);
		printf("\n");
	}
	return(ecnt);
}

upidsets(int npid, int narg, char **arg)
{
   int		pcnt;
   int		ecnt = 0;

	if (narg > 1) {
		vperror("%s:-u option takes comma separated list of pids\n",
			pname);
		return 1;
	}

	if (npid == 0) {
		pidlist[0] = getppid();
		npid = 1;
	}
	for (pcnt = 0; pcnt < npid; pcnt++) {
		if (schedctl(UNSETPSET, pidlist[pcnt]) == -1) {
			vperror("%s: delete process %d from all sets",
				pname, pidlist[pcnt]);
			ecnt++;
			continue;
		}
	}
	return(ecnt);
}

/*
 * Run a command in a set.
 */
void
runset(bvname_t set, int narg, char **prog)
{
   char	*shell;
   char	*args;

	if (schedctl(SETPSET, 0, set) == -1) {
		vperror("%s: join set %s", pname, tosym(set));
		exit(1);
	}
	if ((shell = getenv("SHELL")) == 0)
		shell = "/bin/sh";
	if (narg <= 1)
		execl(shell, shell, 0);
	else
		execvp(prog[0], &prog[0]);
	vperror("%s: execute command \"%s\"", pname, prog[0]);
	exit(1);
}

/*
 * Operate on collections of processes.
 */
int
masspid(bvname_t set1, int narg, char **arg)
{
   struct dirent	*ent;
   int			fd;
   struct prpsinfo	psi;
   char			file[BUFSIZ];
   int			mod = 0;
   bvname_t		set2;

	if (narg == 2) {
		mod = 1;
		set2 = tosname(arg[0]);
	}


	seteuid(0);
	chdir(path);
	rewinddir(dd);
	while (ent = readdir(dd)) {
		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0 ||
		    strcmp(ent->d_name, "pinfo") == 0)
			continue;
		if ((fd = open(ent->d_name, O_RDONLY)) == -1)
			continue;
		if (ioctl(fd, PIOCPSINFO, &psi) == -1) {
			fprintf(stderr, "%s: can't get status of process %s\n",
				pname, ent->d_name);
			close(fd);
			continue;
		}
		if (narg == 0) {
			if (psi.pr_pset)
				printf("%5d %s\n", psi.pr_pid,
					tosym(psi.pr_pset));
		}
		else if (psi.pr_pset == set1) {
			if (mod) {
				if (schedctl(SETPSET, psi.pr_pid, set2)==-1){
					vperror(
					"%s: change process %d to set 5s",
						pname, psi.pr_pid,
						tosym(set2));
					close(fd);
					continue;
				}
				printf("%5d -> %s\n", psi.pr_pid,
					tosym(set2));
			}
			else
				printf("%d\n", psi.pr_pid);
		}
		close(fd);
	}
	seteuid(getuid());
	return(0);
}

/*
 * Show overall system status of processor sets.
 */
void
showstatus()
{
	printf("Active Processors: ");
	printf("%s\n", printset(Runq._active_pset));
	printf("Active processor sets:\n");
	opsets(0, 0, 0);
	printf("\n");
	printf("Restricted processes:\n");
	masspid(0, 0, 0);
	printf("\n");
}

char *
tosym(bvname_t bvn)
{
   static char	sbuf[10];
   char		*n;

	if (bvn == PSET_ALL)
		return("all");
	else if (bvn == PSET_ACTIVE)
		return("active");
	else if (bvn < BV_MAX)
		sprintf(sbuf, "cpu%d", bvn);
	else if (bvn <= PSET_ACTIVE && bvn > (PSET_ACTIVE - BV_MAX))
		sprintf(sbuf, "mrun%d", PSET_ACTIVE - bvn);
	else if (n = ntosym(bvn))
		return(n);
	else
		sprintf(sbuf, "%d", bvn);
	return(sbuf);
}


ulong
tosname(char *a1)
{
   ulong	tn;

	if (a1[0] >= '0' && a1[0] <= '9')
		return(strtoul(a1, 0, 0));
	if (strcmp(a1, "all") == 0)
		return(PSET_ALL);
	if (strcmp(a1, "active") == 0)
		return(PSET_ACTIVE);
	if (strncmp(a1, "cpu", 3) == 0) {
		a1 += 3;
		return(strtoul(a1, 0, 0));
	}
	if (strncmp(a1, "mrun", 4) == 0) {
		a1 += 4;
		return(PSET_ACTIVE - strtoul(a1, 0, 0));
	}
	if (tn = symton(a1))
		return(tn);
	fprintf(stderr, "%s: unknown set name %s\n", pname, a1);
	exit(1);
	/* NOTREACHED */
}

char *
printset(bvinfo_t *tbp)
{
   static char	mbuf[((sizeof(sbv_t)*8)/4)+3];

	sprintf(mbuf, "0x%llx", (tbp->bv_vec & active.bv_vec));
	return(mbuf);
}

void
readsets()
{
   bvinfo_t	*bp;
   bvinfo_t	*lbp = 0;

	if (sets != 0)
		return;

	if ((bp = (bvinfo_t *) sysmp(MP_KERNADDR, MPKA_PSET))==
						(bvinfo_t *)(__psint_t)-1){
		vperror("%s: accessing set list", pname);
		exit(1);
	}
	/* MPKA_PSET returns &Bv in kernel */
	kread(bp, &bp, sizeof(bp));
	sets = 0;
	while (bp != 0) {
		lbp = (bvinfo_t *) calloc(1, sizeof(*lbp));
		kread(bp, lbp, setsz);
		bp = lbp->bv_next;
		lbp->bv_next = sets;
		sets = lbp;
	}
}

bvinfo_t *
findset(unsigned long sn)
{
   bvinfo_t	*lbp = 0;

	if (sn == 0)
		return(0);
	readsets();
	for (lbp = sets; lbp != 0; lbp = lbp->bv_next) {
		if (lbp->bv_name == sn)
			return(lbp);
	}
	return(0);
}

void
getset(char *procs, sbv_t *tset)
{
   int		blen;
   int		ilen;
   char		*str;
   int		i;
   int		j;

	*tset = 0;
	if (procs[0] == '0' && (procs[1] == 'x' || procs[1] == 'X')) {
		procs += 2;
		str = procs;
		i = 0;
		while (*str != '\0') {
			if (!((*str >= '0' && *str <= '9') ||
			      (*str >= 'a' && *str <= 'f') ||
			      (*str >= 'A' && *str <= 'F')))
				return;
			str++;
			i++;
		}
		if (i > (BV_MAX / 4))
			return;
		while (str != procs) 
			--str;
		sscanf(str, "%llx", tset);
		*str = '\0';
	}
	else {
		while (*procs) {
			if (str = strchr(procs, ','))
				*str++ = '\0';
			else
				str = &(procs[strlen(procs)]);
			i = (int) strtoul(procs, 0, 0);
			if (i >= 0 && i < BV_MAX)
				*tset |= (sbv_t)1 << i;
			procs = str;
		}
	}
}

void
kread(addr, buf, len)
   void		*addr;
   void		*buf;
   int		len;
{
	off_t	seekaddr;

#if (_MIPS_SZPTR == 64)
	seekaddr = (__psunsigned_t)addr & 0x7fffffffffffffff;
#else /* _MIPS_SZPTR == 32 */
	seekaddr = (__psunsigned_t)addr & 0x7fffffff;
#endif /* _MIPS_SZPTR == 32 */

	if (lseek(memfd, seekaddr, 0) == -1) {
		fprintf(stderr, "%s: error seeking to %#x on memory\n",
			pname, addr);
		exit(1);
	}
	if (read(memfd, buf, len) != len) {
		fprintf(stderr, "%s: error reading from memory\n", pname);
		exit(1);
	}
}

void
vperror(const char *fmt, ...)
{
   va_list	args;
   char		vpbuf[BUFSIZ];

	va_start(args, fmt);

	vsprintf(vpbuf, fmt, args);
	perror(vpbuf);

	va_end(args);
}

/*
 * Dealing with set names and mappings. The map file is expected to have the
 * following format:
 *
 *	# stuff ...
 *		-> comment line
 *	{symbolic name} {kernel name} {vector} [ignored ...]
 */

typedef struct nmap {
	char		*name;
	sbv_t		vec;
	ulong		iname;
	struct nmap	*next;
} nmap_t;
nmap_t		*nroot = 0;

void
usertab()
{
   char		rbuf[BUFSIZ];
   FILE		*tab = 0;
   char		*tok;
   char		*temp_tok;
   nmap_t	*tmp;
   nmap_t	*last = 0;

	setuid(getuid());
	if ((tab = fopen(dfile, "r")) == NULL)
		exit(0);
	while (fgets(rbuf, BUFSIZ, tab) != NULL) {
		if ((tok = strtok(rbuf, " \t\n")) == NULL)
			continue;
		if (*tok == '#')
			continue;
		tmp = (nmap_t *) calloc(1, sizeof(nmap_t));
		tmp->name = strdup(tok);
		if ((tok = strtok(0, " \t\n")) == NULL) {
			free(tmp->name);
			free(tmp);
			return;
		}
		tmp->iname = strtoul(tok, 0, 0);
		if ((tok = strtok(0, " \t\n")) == NULL) {
			free(tmp->name);
			free(tmp);
			return;
		}
		getset(tok, &tmp->vec);
		if (last) {
			last->next = tmp;
			last = tmp;
		}
		else {
			nroot = tmp;
			last = tmp;
		}
		tmp->next = 0;
	}
	fclose(tab);
	exit(0);
}


void 
readtab()
{
   int		status;

	if (nroot != 0)
		return;
	if (sprocsp((void (*)(void *, size_t))usertab, PR_SADDR, 0, NULL, STACKSIZE) == -1)
		return;
	wait(&status);
	if (status != 0)
		exit(1);
}


void
inittab()
{
   nmap_t	*np;
   int		err = 0;

	np = nroot;
	while (np) {
		if (sysmp(MP_PSET, MPPS_CREATE, np->iname, &np->vec) == -1) {
			vperror("%s: create set %s", pname, np->name);
			err++;
		}
		else if (verbose)
			fprintf(stderr, "%s: created set %s\n", pname, 
				np->name);
		np = np->next;
	}
	if (err)
		exit(1);
}

int
symtovec(char *n, sbv_t *res)
{
   nmap_t	*np;
   int		i;

	np = nroot;
	while (np != 0) {
		if (strcmp(n, np->name) == 0) {
			*res = np->vec;
			return(1);
		}
		np = np->next;
	}
	return(0);
}

char *
ntosym(ulong sym)
{
   nmap_t	*np;

	np = nroot;
	while (np != 0) {
		if (sym == np->iname)
			return(np->name);
		np = np->next;
	}
	return(0);
}

ulong
symton(char *sym)
{
   nmap_t	*np;

	np = nroot;
	while (np != 0) {
		if (strcmp(sym, np->name) == 0)
			return(np->iname);
		np = np->next;
	}
	return(0);
}
