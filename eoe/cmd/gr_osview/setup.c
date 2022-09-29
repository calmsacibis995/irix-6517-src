/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Parse the options and description files and set up for display.
 */

# include	"grosview.h"

# include	<sys/sysmp.h>
# include	<sys/ioctl.h>
# include	<sys/socket.h>
# include	<sys/prctl.h>
# include	<sys/wait.h>

# include	<fcntl.h>
# include	<string.h>
# include	<gl.h>
# include	<math.h>
# include	<stdlib.h>
# include	<unistd.h>

# define	GROSFILE	"GROSVIEW"
# define	DEFFILE		".grosview"
# define	VERSION		"2.2"

/*
 * Lexical analyzer constants.
 */
# define 	TOK_MOD		1
# define 	TOK_ARG		2
# define	TOK_NONE	3

barlist_t	*barlist = 0;	/* ordered list of bars to display */
barlist_t	*barend = 0;	/* end of the list */
static char	*desfile = 0;	/* description file */

# define	MY_BLUE		BLUE
# define	MY_RED		RED
# define	MY_MAGENTA	MAGENTA
# define	MY_GREEN	GREEN
# define	MY_CYAN		CYAN
# define	MY_YELLOW	YELLOW

Colorindex	colorlist[MAXSECTS] = {
	MY_BLUE, MY_RED, MY_MAGENTA, MY_GREEN, MY_CYAN, MY_YELLOW
};
numcol_t		limcol = { CNULL, CNULL };
numcol_t		maxcol = { BLACK, RED };
numcol_t		sumcol = { WHITE, BLUE };

/*
 * Global options.
 */
int		do_border = 1;	/* draw the window border */
int		debug = 0;	/* enable debugging output */
int		debugger = 0;	/* stay in foreground */
int		pinning = 0;	/* if want to pin critical memory */
FILE		*validscript = 0;
char		sexit = 0;
char		*cscript = 0;	/* create a script file */
FILE		*scriptfd = 0;	/* hidden restart script descriptor */
FILE		*logfd;		/* explicit script descriptor */
int		slave = 0;	/* run as a slave */
char		*remotehost = 0;/* run on remote host */
char		*pname;		/* program name */
int		barborder = 1;	/* no borders on the bars */
int		sigfreeze = 0;	/* freeze window on signal */
int		setupstdin = 0;	/* setup came on standard input */
char		*sname = 0;

int		prefpos = 0;	/* if want position */
int		prefxpos;
int		prefypos;

int		prefwsize = 0;	/* if want size */
int		prefxsize;
int		prefysize;

dstat_t 	dstat[MAXDISK];		/* the disks */
int		ndisknames = 0;		/* number of disks */
char		nofont = 0;		/* if font changed */
char		gi_swap = 0;		/* get swap information */
char		gi_tcp = 0;		/* get TCP information */
char		gi_udp = 0;		/* get UDP information */
char		gi_ip = 0;		/* get IP information */
char		gi_if = 0;		/* get if_ layer information */

char		*vmunix	= "/unix"; /* namelist file */

int		printpos = 0;	/* if print position wanted */
int		myvers = CURVERS;

/*
 * The default setup that will be used.  Don't add carriage returns,
 * the code does that for you.
 */
static char	*defsetup[] = {
	"cpu",
	0
};
static char	*jmbsetup[] = {
	"cpu",
	"rmem max tracksum",
	"wait",
	"sysact max",
	"gfx",
	0
};
static int	useint = 0;

char		**intlist;

static char *		getarg(char *);
static void		lnkbar(barlist_t *);
static void		openthread(void *);
static int		gettok(char **);
static void		pushtok(void);
static int		globalopts(void);
static int		makectab(char *);
static void		scanfile(char *);
static void		sendfile(char *);
static FILE *		uidopen(char *, char *);
static void		printversion(void);
static void		huntspec(int);
static int		dokey(char *, barlist_t **);
static int		docompat(char *);
static int		doopts(char *, barlist_t *);
static int		breakline(char *s);
static int		dokey(char *, barlist_t **);

/*
 * Overall setup - parse args.
 * If we are the display side of a remote operation, we'll never return
 */
void
setup(int ac, char *av[])
{
   extern int	optind;
   extern char	*optarg;
   int		c;
   int		err;
   int		i;

	pname = av[0];
	err = 0;
	intlist = defsetup;
	while ((c = getopt(ac, av, "FEes:LhazpD:VN:Rdjn:")) != EOF) {
	switch(c) {
	case 'F':	/* suppress text */
		nofont = 1;
		break;
	case 'E':	/* data generator */
		slave = 1;
		cscript = "no chance";
		debugger = 1;
		break;
	case 'L':	/* pin process in memory */
		pinning = 1;
		break;
	case 'p':	/* print window position and size */
		printpos = 1;
		break;
	case 'a':	/* use "jim's favorite" setup */
		intlist = jmbsetup;
		useint = 1;
		break;
	case 'z':	/* freeze program */
		sigfreeze++;
		break;
	case 'e':	/* exit at script end */
		sexit = 1;
		break;
	case 'D':	/* setup file name */
		desfile = optarg;
		break;
	case 's':	/* create script file */
		cscript = optarg;
		break;
	case 'V':	/* output program version */
		printversion();
		exit(0);
	case 'R':	/* slave or recorder mode */
		slave++;
		break;
	case 'd':	/* enable debugging */
		debug++;
		break;
	case 'j':
		debugger++;
		break;
	case 'h':	/* print help screen */
		err++;
		break;
	case 'N':	/* run on remote host */
		remotehost = optarg;
		break;
	case '?':
		err++;
		break;
        case 'n':
                vmunix = optarg;
                break;
	}}

	/*
	 * Consistency checks.
	 */
	if (err) {
		fprintf(stderr, "usage: \"%s\" followed optionally by:\n",
			pname);
		fprintf(stderr,
			" -a          => use gr_osview enhanced default\n");
		fprintf(stderr, " -s file     => create an export file\n");
		fprintf(stderr,
			" -p          => dump window position and size\n");
		fprintf(stderr, " -z          => enable freeze toggling\n");
		fprintf(stderr, " -E          => run only as data generator\n");
		fprintf(stderr, " -e          => exit at end of export file\n");
		fprintf(stderr, " -h          => this display\n");
		fprintf(stderr,
			" -N [user@]host => monitor given host\n");
		fprintf(stderr,
			" -D desfile  => take setup from given file\n");
		fprintf(stderr,
			" -L          => lock needed program code and data in memory\n");
		fprintf(stderr,
			" -V          => print program version info\n");
		exit(1);
	}
	if (slave && remotehost) {
		fprintf(stderr, "can't be slave and master at same time\n");
		exit(1);
	}
	for (i = optind; i < ac; i++) {
		/*
		 * Ignore any extra arguments for now
		 */
	}

	/*
	 * If remote operation is requested, try to contact the host
	 * early so we can have it's parameters around for setup.
	 */
	if (remotehost) {
	   char		*fd;

		fd = tmpnam((char *) 0);
		scriptfd = uidopen(fd, "w+");
		setbuf(scriptfd, NULL);
		if (!debug)
			unlink(fd);
		else
			printf("setup: script file is \"%s\"\n", fd);
		fprintf(scriptfd, EXPORTSTR);
		fputs("\n", scriptfd);
	}
	if (cscript && !slave) {
		if ((logfd = uidopen(cscript, "w")) == NULL) {
			fprintf(stderr, "%s: can't create export file\n",
				pname);
			exit(1);
		}
		fprintf(logfd, EXPORTSTR);
		fputs("\n", logfd);
		fflush(logfd);
	}
	if (remotehost)
		callremote(0);
	else if (slave) {
		if (!cscript) {
			printf("%d %d\n", S_VERSION, myvers);
			fflush(stdout);
		}
	}
	cbpreops();
	huntspec(0);
}

static int
makectab(char *s)
{
   char		*t;
   int		index;
   int		ctabi;

	if ((t = strtok(s, ",")) == NULL)
		return(1);
	ctabi = 0;
	while (t != NULL) {
		if (ctabi >= MAXSECTS)
			return(0);
		if ((index = strtol(t, (char **) 0, 0)) < 0 || index >= 4096) {
			fprintf(stderr, "%s: invalid colortable index %d\n",
				pname, index);
			return(1);
		}
		colorlist[ctabi++] = index;
		t = strtok(0, ",");
	}
	return(0);
}

static void
printversion(void)
{
	printf("gr_osview version %s\n", VERSION);
	printf(
	   "Copyright (c) 1989-1993 Silicon Graphics Computer Systems\n");
	printf("All Rights Reserved\n");
}

/*
 * Setup options from description file.
 */
static void
huntspec(int x)
{
   char		*efile;
   char		fbuf[BUFSIZ];

	stg_open();

	/*
	 * If explicit file identified, use that one.
	 */
	if (desfile) {
		if (x && strcmp(desfile, "-") == 0)
			return;
		if (remotehost)
			sendfile(desfile);
		else
			scanfile(desfile);
		return;
	}

	/*
	 * If favorite setup is asked for, do it instead.
	 */
	if (useint) {
		if (remotehost)
			sendfile((char *) 0);
		else
			scanfile((char *) 0);
		return;
	}

	/*
	 * If file name set in environment, use that one.
	 */
	if ((efile = getenv(GROSFILE)) != 0) {
		if (remotehost)
			sendfile(efile);
		else
			scanfile(efile);
		return;
	}

	/*
	 * Finally, check home directory for a file.
	 */
	if ((efile = getenv("HOME")) == 0)
		strcpy(fbuf, "./");
	else {
		strcpy(fbuf, efile);
		strcat(fbuf, "/");
	}
	strcat(fbuf, DEFFILE);
	if (access(fbuf, 04) != -1) {
		if (remotehost)
			sendfile(fbuf);
		else
			scanfile(fbuf);
		return;
	}

	/*
	 * Ok, no file.  Use the boring default setup.
	 */
	if (remotehost)
		sendfile((char *) 0);
	else
		scanfile((char *) 0);
}

/*
 * Scan a file for interesting setup lines.  Blow cookies on error.
 */

int
sgets(char *b, int s, FILE *f)
{
   static int	lineno = 0;
   int		ret;

	if (f == (FILE *) 0) {
		if (intlist[lineno] == 0) {
			ret = 0;
			lineno = 0;
		}
		else {
			strncpy(b, intlist[lineno], s);
			strcat(b, "\n");
			lineno++;
			ret = 1;
		}
	}
	else {
		if (debug)
			fprintf(stderr, "sgets: waiting for string\n");
		if (fgets(b, s, f) == NULL)
			ret = 0;
		else
			ret = 1;
		if (ret)
			if (strcmp(b, "EOF\n") == 0)
				ret = 0;
	}
	if (debug) {
		if (ret) {
		   char		*s;
		   char		copybuf[BUFSIZ];

			strcpy(copybuf, b);
			if ((s = strchr(copybuf, '\n')) != NULL)
				*s = '\0';
			fprintf(stderr, "*SR> read <%s>\n", copybuf);
		}
		else
			fprintf(stderr, "*SR> EOF\n");
	}
	return(ret);
}

static void
scanfile(char *s)
{
   char		rdbuf[BUFSIZ];
   char		*sbuf;
   FILE		*fd;
   char		*t;
   barlist_t	*bp;
   long		line;
   int		ttype;

	if (s == (char *) 0)
		fd = (FILE *) 0;
	else if (strcmp(s, "-") == 0) {
		setupstdin = 1;
		fd = stdin;
	}
	else if ((fd = uidopen(s, "r")) == NULL) {
		fprintf(stderr, "%s: unable to open description file \"%s\"\n",
			pname, s);
		exit(1);
	}
	line = 0;
	while (sgets(rdbuf, BUFSIZ, fd) != NULL) {
		if (line == 0 && strncmp(rdbuf, EXPORTSTR, strlen(EXPORTSTR))
			== 0) {
			if (debug)
				fprintf(stderr, "input is an export file\n");
			if (scanscript(fd)) {
				fprintf(stderr,
					"%s: format error in export file\n",
					pname);
				exit(1);
			}
			if (!slave)
				validscript = fd;
			return;
		}
		sbuf = rdbuf;
		line++;
		while (*sbuf != '\0' && (*sbuf == ' ' || *sbuf == '\t' ||
			*sbuf == '\n')) sbuf++;
		if (*sbuf == '\0')
			continue;
		if (*sbuf == '#')
			continue;
		if (breakline(sbuf)) {
			fprintf(stderr, "line %d: syntax error\n", line);
			exit(1);
		}
		ttype = gettok(&t);
		if (ttype) {
			if (ttype != TOK_MOD)
				goto badline;
			if (*t == '#')
				continue;
			if (dokey(t, &bp)) {
				if (docompat(t)) {
					fprintf(stderr,
					"%s: %s bar no longer supported\n",
						pname, t);
					free(bp);
					bp = 0;
					continue;
				}
				goto badline;
			}
			while ((ttype = gettok(&t)) != 0) {
				if (ttype != TOK_MOD)
					goto badline;
				if (doopts(t, bp))
					goto badline;
			}
		}
	}
	if (fd != (FILE *) 0 && fd != stdin)
		fclose(fd);
	if (barlist == 0) {
		fprintf(stderr, "%s: nothing to display\n", pname);
		exit(1);
	}
	if (debug)
		fprintf(stderr, "scanfile completed\n");
	return;

badline:
	fprintf(stderr,
		"%s: description file format error on line %ld\n",
		pname, line);
	exit(1);
}

/*
 * Push a file over the wire to a remote gr_osview.
 * NOTE: display(master) side never returns from here.
 */
static void
sendfile(char *s)
{
   FILE		*fd;
   char		rdbuf[BUFSIZ];
   pid_t	pid;

	/*
	 * Get file prepped to run.
	 */
	if (s == (char *) 0)
		fd = (FILE *) 0;
	else if (strcmp(s, "-") == 0) {
		setupstdin = 1;
		fd = stdin;
	}
	else if ((fd = uidopen(s, "r")) == NULL) {
		fprintf(stderr, "%s: unable to open description file \"%s\"\n",
			pname, s);
		exit(1);
	}

	/*
	 * The parent goes on and becomes the RPC daemon, while the child
	 * sends the file and then goes away.
	 */
	if ((pid = fork()) > 0) {
		passdata();
		exit(1);
	} else if (pid < 0) {
		fprintf(stderr, "%s: unable to fork\n", pname);
		exit(1);
	}

	/*
	 * Now send the spec file over the wire.
	 */
	if (debug)
		printf("sendfile: sending the data\n");
	while (sgets(rdbuf, BUFSIZ, fd) != 0)
		fprintf(rmtwrite, "%s", rdbuf);
	fprintf(rmtwrite, "EOF\n");
	fflush(rmtwrite);
	if (debug)
		printf("sendfile: done\n");
	if (fd != (FILE *) 0)
		fclose(fd);

	/*
	 * bye, bye!
	 */
	exit(0);
}

/*
 * Actual routines to do the recognition.
 */

# define	easyway(s,e)	if(eq(s,t)){bp->s_btype=e;lnkbar(bp);return(0);}
# define	feasyway(s,e,f)	if(eq(s,t)){bp->s_btype=e;gi_ ##f=1;\
			lnkbar(bp);return(0);}


static int
dokey(char *t, barlist_t **pbp)
{
   extern	int	isgfx(void);
   char		*s;
   int		ntype;
   barlist_t	*bp;

	if (eq(t, "opt")) {
		/*
		 * Introduce a line of options.
		 */
		*pbp = 0;
		return(globalopts());
	}

	*pbp = bp = newbar();
	if (eq(t, "cpu")) {
	   int		numcpu;

		/*
		 * Bring up a CPU monitor bar.
		 */
		bp->s_btype = bcpu;
		if ((ntype = gettok(&s)) != TOK_ARG) {
			/*
			 * No arguments - monitor all CPU's separately.
			 */
			bp->s_subtype = CPU_ALL;
			if (ntype != 0)
				pushtok();
		}
		else if (strcmp(s, "sum") == 0)
			/*
			 * Sum all performance for a single bar.
			 */
			bp->s_subtype = CPU_SUM;
		else {
			/*
			 * Monitor just the given CPU.
			 */
			bp->s_subtype = strtol(s, (char **) 0, 0);
			numcpu = sysmp(MP_NPROCS);
			if (bp->s_subtype >= numcpu || bp->s_subtype < 0) {
				fprintf(stderr, "%s: invalid CPU number %d\n",
					pname, bp->s_subtype);
				return(1);
			}
		}
		lnkbar(bp);
		return(0);
	}
	easyway("rmem", bmem);		/* real memory usage */
	if (eq(t, "rmemc")) {           /* real memory usage w/ cache stats */
		bp->s_btype = bmemc;
		bp->s_interval = 5;     /* interval(5) */
		bp->s_upmove = 0.4;     /* attack(0.4) */
		lnkbar(bp);
		return(0);
	}
	easyway("wait", bwait);		/* CPU wait monitor */
	easyway("sysact", bsyscall);	/* system calls */
        if (eq(t, "gfx")) {
	       if (!isgfx()) {		/*  no graphics - ignore */
		       free((void *)bp);
		} else {
		        bp->s_btype = bgfx;
			lnkbar(bp);
		}
	        return(0);
	}
	easyway("iothru", biothru);	/* read/write io throughput */
	easyway("bdev", bbdev);		/* block device activity */
#ifdef bufh_bar
	easyway("bufh", bbufh);		/* buffer headers */
#endif
	easyway("intr", bintr);		/* interrupt acivity */
	easyway("fault", bfault);	/* page fault activity */
	easyway("tlb", btlb);		/* TLB flush activity */
	easyway("pswap", bswap);	/* swap device activity */
	feasyway("swp", bswps,swap);	/* swap space available */
	feasyway("netudp", bnetudp,udp);	/* UDP packets */
	feasyway("netip", bnetipr,ip);	/* IP packets */
	feasyway("nettcp", bnet,tcp);	/* TCP packets */
	feasyway("net", bnet,tcp);	/* network activity */
	if (eq(t, "netif")) {
		bp->s_btype = bnetip;
		gi_if = 1;
		if ((ntype = gettok(&s)) != TOK_ARG) {
			/*
			 * No arguments.  We want to monitor all IP
			 * interfaces separately.
			 */
			if (ntype != 0)
				pushtok();
			bp->s_subtype = IP_ALL;
			lnkbar(bp);
			return(0);
		}
		else if (eq(s, "sum")) {
			/*
			 * Just sum the information over all the interfaces.
			 */
			bp->s_subtype = IP_SUM;
			lnkbar(bp);
			return(0);
		}
		/*
		 * Otherwise, monitor just the given interface.
		 */
		if ((bp->s_subtype = netifinfo(s)) < 0)
			return(1);
		bp->s_name = strdup(s);
		lnkbar(bp);
		return(0);
	}
	if (eq(t, "disk")) {

		/*
		 * Monitor disk capacity.
		 */
		bp->s_btype = bdisk;
		if ((ntype = gettok(&s)) != TOK_ARG)
			return(1);
		if ((bp->s_disk = diskinfo(s)) < 0)
			return(1);
		bp->s_name = strdup(s);
		lnkbar(bp);
		return(0);
	}
	return(1);
}

static int
docompat(char *t)
{
	if (eq("bbuf", t)) return 1;
	if (eq("tlbfault", t)) return 1;
	if (eq("tlbflush", t)) return 1;
	return 0;
}


static int
doopts(char *t, barlist_t *bp)
{
   int		ntype;
   int		ncol;
   int		pcol;
   char		*s;
   int		ci;

	if (eq(t, "color") || eq(t, "colors")) {
		/*
		 * Set the colors for the bar sections.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		ci = 0;
		s = getarg(t);
		while (s != 0 && ci < MAXSECTS) {
			bp->s_colors[ci++] = strtol(s, (char **) 0, 0);
			s = getarg((char *) 0);
		}
		bp->s_ndcolors = ci;
		return(0);
	}
	if (eq(t, "attack")) {
	   double	f;

		/*
		 * Adjust attack span - maximum amount of movement
		 * up for a single sample.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		f = atof(t);
		if (f <= 0)
			return(1);
		bp->s_upmove = f;
		return(0);
	}
	if (eq(t, "decay")) {
		/*
		 * Parameter is no longer used.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		return(0);
	}
	if (eq(t, "interval")) {
	   int		niv;

		/*
		 * Set the new update interval to use.  Specified in units
		 * of the base interval frequency.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		if ((niv = strtol(t, (char **) 0, 0)) <= 0)
			return(1);
		bp->s_interval = niv;
		return(0);
	}
	if (eq(t, "tracksum")) {
	   int		niv;

		/*
		 * Set the average interval to use.  Specified in units
		 * of the base interval frequency.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG) {
			bp->s_flags |= SF_AVERAGE;
			bp->s_average = 0;
			if (ntype != 0)
				pushtok();
			return(0);
		}
		if ((niv = strtol(t, (char **) 0, 0)) <= 0)
			return(1);
		bp->s_average = niv;
		bp->s_flags |= SF_AVERAGE;
		return(0);
	}
	if (eq(t, "noborder")) {
		/*
		 * Suppress the border around an individual bar.
		 */
		bp->s_flags |= SF_NOBORD;
		return(0);
	}
	if (eq(t, "ticks")) {
	   int		slt;
	   char		*s;
		/*
		 * Generate ticks on a strip chart.  If a single argument is
		 * given, it is a request to paint tick marks on the
		 * strip chart every N intervals.  If a second argument is
		 * given, then it is a request for a fat tick mark every
		 * M of the regular ticks marks.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		if ((s = getarg(t)) == 0)
			return(1);
		if ((slt = strtol(s, (char **) 0, 0)) <= 0)
			return(1);
		bp->s_flags |= SF_TICK;
		bp->s_stevery = slt;

		if ((s = getarg((char *) 0)) == 0)
			return(0);
		if ((slt = strtol(s, (char **) 0, 0)) <= 0)
			return(1);
		bp->s_flags |= SF_BIGTICK;
		bp->s_stbig = slt;
		return(0);
	}
	if (eq(t, "strip")) {
		/*
		 * Generate a strip chart bar.
		 */
		bp->s_flags |= SF_STRIP;
		return(0);
	}
	if (eq(t, "samples")) {
		/*
		 * Number of samples for strip chart.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		if ((bp->s_nsamps = strtol(t, (char **) 0, 0)) <= 0)
			return(1);
		return(0);
	}
	if (eq(t, "max")) {
	   int		niv;

		/*
		 * Set the maximum interval to use.  Specified in units
		 * of the base interval frequency.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG) {
			bp->s_flags |= SF_MAX;
			bp->s_max = 0;
			if (ntype != 0)
				pushtok();
			return(0);
		}
		if ((niv = strtol(t, (char **) 0, 0)) <= 0)
			return(1);
		bp->s_max = niv;
		bp->s_flags |= SF_MAX;
		return(0);
	}
	if (eq(t, "numeric")) {
		/*
		 * Don't want our fancy display.
		 */
		bp->s_flags |= SF_NUM;
		return(0);
	}
	if (eq(t, "lockscale")) {
	   char		*s;

		/*
		 * Lock the bar scale to a particular value.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		if ((bp->s_limit = strtol(t, (char **) 0, 0)) <= 0)
			return(1);
		s = t + strlen(t) - 1;
		if (*s == 'K' || *s == 'k')
			bp->s_limit *= 1024;
		else if (*s == 'M' || *s == 'm')
			bp->s_limit *= (1024*1024);
		else if (*s == 'G' || *s == 'g')
			bp->s_limit *= (1024*1024*1024);
		bp->s_flags |= SF_SLOCK;
		return(0);
	}
	if (eq(t, "creepscale")) {
		/*
		 * Autoscaling only goes up; overridden by lockscale
		 */
		bp->s_flags |= SF_CREEP;
		return(0);
	}
	if (eq(t, "maxcolor")) {
	   char		*s;

		/*
		 * Set the color scheme for maximums.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		if ((s = getarg(t)) == 0)
			return(1);
		if ((ncol=strtol(s,(char **)0,0))<0||ncol>=4096)
			return(1);
		if ((s = getarg((char *) 0)) == 0)
			return(1);
		if ((pcol=strtol(s,(char **)0,0))<0||pcol>=4096)
			return(1);
		bp->s_maxcol.back = ncol;
		bp->s_maxcol.front = pcol;
		return(0);
	}
	if (eq(t, "limcolor")) {
	   char		*s;

		/*
		 * Set the color scheme for scale limits.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		if ((s = getarg(t)) == 0)
			return(1);
		if ((ncol=strtol(s,(char **)0,0))<0||ncol>=4096)
			return(1);
		if ((s = getarg((char *) 0)) == 0)
			return(1);
		if ((pcol=strtol(s,(char **)0,0))<0||pcol>=4096)
			return(1);
		bp->s_limcol.back = ncol;
		bp->s_limcol.front = pcol;
		return(0);
	}
	if (eq(t, "sumcolor")) {
	   char		*s;

		/*
		 * Set the color scheme for tracking sums.
		 */
		if ((ntype = gettok(&t)) != TOK_ARG)
			return(1);
		if ((s = getarg(t)) == 0)
			return(1);
		if ((ncol=strtol(s,(char **)0,0))<0||ncol>=4096)
			return(1);
		if ((s = getarg((char *) 0)) == 0)
			return(1);
		if ((pcol=strtol(s,(char **)0,0))<0||pcol>=4096)
			return(1);
		bp->s_sumcol.back = ncol;
		bp->s_sumcol.front = pcol;
		return(0);
	}
	return(1);
}

static int
globalopts(void)
{
   int		ttype;
   int		ncol;
   int		pcol;
   char		*t;

	while ((ttype = gettok(&t)) != 0) {
		if (ttype != TOK_MOD)
			return(1);
		if (eq(t, "width")) {
			/*
			 * Set number of bars in width of window 
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			if ((width = strtol(t, (char **) 0, 0)) < 0 ||
			    width > 10)
				return(1);
		}
		else if (eq(t, "winframe"))
			/*
			 * Draw a border around the window.
			 */
			do_border = 1;
		else if (eq(t, "nodecorate"))
			/*
			 * Don't draw a border around the window.
			 */
			do_border = 0;
		else if (eq(t, "noborder"))
			/*
			 * Suppress the border around a single bar
			 */
			barborder = 0;
		else if (eq(t, "colors")) {
			/*
			 * Set the default colors for all bars
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			return(makectab(t));
		}
		else if (eq(t, "backcolor")) {
			/*
			 * Set the background color.
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			if ((int)(backcolor = strtol(t, (char **) 0, 0)) < 0 ||
			    backcolor >= 4096)
				return(1);
		}
		else if (eq(t, "maxcolor")) {
		   char		*s;

			/*
			 * Set the color scheme for maximums.
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			if ((s = getarg(t)) == 0)
				return(1);
			if ((ncol=strtol(s,(char **)0,0))<0||ncol>=4096)
				return(1);
			if ((s = getarg((char *) 0)) == 0)
				return(1);
			if ((pcol=strtol(s,(char **)0,0))<0||pcol>=4096)
				return(1);
			maxcol.back = ncol;
			maxcol.front = pcol;
		}
		else if (eq(t, "limcolor")) {
		   char		*s;

			/*
			 * Set the color scheme for scale limits.
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			if ((s = getarg(t)) == 0)
				return(1);
			if ((ncol=strtol(s,(char **)0,0))<0||ncol>=4096)
				return(1);
			if ((s = getarg((char *) 0)) == 0)
				return(1);
			if ((pcol=strtol(s,(char **)0,0))<0||pcol>=4096)
				return(1);
			limcol.back = ncol;
			limcol.front = pcol;
		}
		else if (eq(t, "sumcolor")) {
		   char		*s;

			/*
			 * Set the color scheme for tracking sums.
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			if ((s = getarg(t)) == 0)
				return(1);
			if ((ncol=strtol(s,(char **)0,0))<0||ncol>=4096)
				return(1);
			if ((s = getarg((char *) 0)) == 0)
				return(1);
			if ((pcol=strtol(s,(char **)0,0))<0||pcol>=4096)
				return(1);
			sumcol.back = ncol;
			sumcol.front = pcol;
		}
		else if (eq(t, "frontcolor")) {
			/*
			 * Set the foreground color
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			if ((int)(frontcolor = strtol(t, (char **) 0, 0)) < 0 ||
			    frontcolor >= 4096)
				return(1);
		}
		else if (eq(t, "interval")) {
			/*
			 * Set the default update interval.  Specified in
			 * tenths of a second.
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			if ((interval = strtol(t, (char **) 0, 0)) <= 0)
				return(1);
		}
		else if (eq(t, "arbsize"))
			/*
			 * Allow arbitrary sizing of the window.
			 */
			arbsize++;
		else if (eq(t, "font")) {
			/*
			 * Set the font to use.
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			fontface = strdup(t);
		}
		else if (eq(t, "wintitle")) {
			/*
			 * Set the window title
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			sname = strdup(t);
		}
		else if (eq(t, "origin")) {
		   char		*s;

			/*
			 * Set the window origin.
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			if ((s = getarg(t)) == 0)
				return(1);
			if ((prefxpos = strtol(s, (char **) 0, 0)) < 0 ||
			    prefxpos > 1275)
				return(1);
			if ((s = getarg((char *) 0)) == 0)
				return(1);
			if ((prefypos = strtol(s, (char **) 0, 0)) < 0 ||
				prefypos > 1020)
				return(1);
			prefpos = 1;
		}
		else if (eq(t, "winsize")) {
		   char		*s;

			/*
			 * Set the window size.
			 */
			if (gettok(&t) != TOK_ARG)
				return(1);
			if ((s = getarg(t)) == 0)
				return(1);
			if ((prefxsize = strtol(s, (char **) 0, 0)) < 0 ||
			    prefxsize > 1275)
				return(1);
			if ((s = getarg((char *) 0)) == 0)
				return(1);
			if ((prefysize = strtol(s, (char **) 0, 0)) < 0 ||
				prefysize > 1020)
				return(1);
			prefwsize = 1;
		}
		else
			return(1);
	}
	return(0);
}

static void
lnkbar(barlist_t *bp)
{
	if (barlist == 0)
		barlist = barend = bp;
	else
		barend = barend->s_next = bp;
}

/*
 * Utilities.
 */
barlist_t *
newbar(void)
{
   barlist_t	*tmp;
   int		i;

	if ((tmp = (barlist_t *) calloc(sizeof(barlist_t), 1)) == 0) {
		fprintf(stderr, "%s: out of memory\n", pname);
		exit(1);
	}
	for (i = 0; i < MAXSECTS; i++)
		tmp->s_colors[i] = colorlist[i];
	tmp->s_limcol.back = CNULL;
	tmp->s_limcol.front = CNULL;
	tmp->s_maxcol.back = CNULL;
	tmp->s_maxcol.front = CNULL;
	tmp->s_sumcol.back = CNULL;
	tmp->s_sumcol.front = CNULL;
	return(tmp);
}

FILE	*resfd = 0;

static void
openthread(void *arg)
{
   char		**args = arg;

	setgid(getgid());
	setuid(getuid());
	resfd = fopen(args[0], args[1]);
}

FILE *
uidopen(char *fn, char *wh) {
   char		*args[3];
   pid_t	pid;
   int		status;

	args[0] = fn;
	args[1] = wh;
	args[2] = 0;
	if ((pid = sproc(openthread, PR_NOLIBC|PR_SADDR|PR_SFDS, args)) == -1)
		return(NULL);
	if (wait(&status) != pid)
		fprintf(stderr, "%s: reaped unknown child\n", pname);
	if (resfd == 0) {
		fprintf(stderr, "%s : file \"%s\": ", pname, fn);
		perror("");
		return(NULL);
	}
	return(resfd);
}

# define MAX_TOKLEN	100
# define MAX_TOKNUM	100

typedef struct tmp100 {
	char		tok[MAX_TOKLEN];
	int		coff;
	int		ttype;
} tok_t;

tok_t	toklist[MAX_TOKNUM];
int	tokmax;
int	nexttok;

static int
breakline(register char *s)
{

   register char	*t;
   register char	*u;
   register char	*v;
   register int		depth;
   register int		spanlen;
   register int		toklen;

	tokmax = 0;
	nexttok = 0;
	while (*s != '\0') {
		/*
		 * Split off a token.
		 */
		spanlen = strspn(s, " \t\n");
		s += spanlen;
		if (*s == '\0')
			return(0);
		toklen = 0;
		t = s;
		while (*t != '\0' && *t != ' ' && *t != '\t' && *t != '\n') {
			t++;
			toklen++;
		}
		t = s;
		for (spanlen = 0; spanlen < toklen; spanlen++)
			if (t[spanlen] == '(')
				break;
		/*
		 * After all this:
		 *    t       = beginning of token
		 *    toklen  = length of token
		 *    spanlen = offset from 't' of argument, if any
		 */
		/*
		 * Check for a modifier argument.
		 */
		if (spanlen < toklen) {
			/*
			 * If this is a modifier argument, and the modifier
			 * is part of the token, split the modifier off
			 * before parsing the argument.
			 */
			if (spanlen > 0) {
				strncpy(toklist[tokmax].tok, t, spanlen);
				toklist[tokmax].tok[spanlen] = '\0';
				toklist[tokmax].ttype = TOK_MOD;
				tokmax++;
			}
			/*
			 * Build up the argument through matching close
			 * parenthesis.
			 */
			u = &t[spanlen];
			depth = 1;
			u++;
			v = toklist[tokmax].tok;
			while (*u != '\0' && depth != 0) {
				switch (*u) {
				case ')':
					depth--;
					if (depth > 0)
						*v++ = ')';
					break;
				case '(':
					depth++;
					*v++ = '(';
					break;
				case '\n':
					return(1);
				case ' ':
				case '\t':
					break;
				default:
					*v++ = *u;
				}
				u++;
			}
			/*
			 * Verify that we actually collected a complete
			 * expression.
			 */
			if (depth != 0)
				return(1);
			*v = '\0';
			toklist[tokmax].ttype = TOK_ARG;
			s = u;
			tokmax++;
		}
		/*
		 * Otherwise, this is a standalone modifier argument.
		 * Collect it and stuff it.
		 */
		else {
			strncpy(toklist[tokmax].tok, t, toklen);
			toklist[tokmax].tok[toklen] = '\0';
			s += toklen;
			toklist[tokmax].ttype = TOK_MOD;
			tokmax++;
		}
	}
	return(0);
}

static int
gettok(register char **s)
{
   register int		ttype;

	if (nexttok >= tokmax)
		return(0);
	else {
		*s = toklist[nexttok].tok;
		ttype = toklist[nexttok].ttype;
		nexttok++;
		return(ttype);
	}
}

static void
pushtok(void)
{
	if (nexttok == 0)
		return;
	nexttok--;
}

static char *
getarg(register char *s)
{
   register char	*t;
   static char		argbuf[100];

	if (s == 0)
		t = strtok(0, ",");
	else {
		strcpy(argbuf, s);
		t = strtok(argbuf, ",");
	}
	if (t == NULL)
		return((char *) 0);
	else
		return(t);
}
