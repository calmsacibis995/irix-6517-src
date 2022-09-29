#ident "$Revision: 1.14 $"

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)dumpmain.c	5.6 (Berkeley) 2/23/87";
 */

#define _DUMPMAIN_C_	1

#include "dump.h"
#include "dumprmt.h"

int	ntrec = NTREC;	/* # tape blocks in each tape record */
long	dev_bsize = DEV_BSIZE;	/* recalculated below */
int	dofork = 1;
int	bsdformat = 1;	/* directory dumping format */

static ulong	capacity;	/* capacity in Megabytes */
static char	*cmdline, *eocmdline;
static int	etapes;		/* estimated number of tapes */

static void sigAbort(void);
static void sigbus();
static void sigfpe();
static void sighup();
static void sigsegv();
static void sigterm();
static void sigtrap();
static void usage(void);

int
main(int argc, char *argv[])
{
	char		*arg;
	int		bflag = 0, i;
	float		fetapes;
	struct	mntent	*dt;
	struct stat	statb;
	int		tryagain = 1;
	char		*orig_disk = NULL;

	time(&(spcl.c_date));
	cmdline = &argv[0][0];
	eocmdline = cmdline;
	for (i = 0; i < argc; ++i)
		eocmdline += strlen(argv[i]) + 1;

	tsize = 0;	/* Default later, based on 'c' option for cart tapes */
	tape = TAPE;
	disk = NULL;
	increm = NINCREM;
	incno = '9';
	uflag = 0;

	if ((argc == 1) || ((argc == 2) && (strcmp(*(argv+1), "w") != 0) &&
			(strcmp(*(argv+1), "W") != 0))) {
		uflag++;
		goto nooptions;	/* last option is always the filesystem */
	} else {
		argv++;
		argc--;
		arg = *argv;
	}
	while(*arg)
	switch (*arg++) {
#ifdef notdef
	case 'e':
		/*
		 * Dumping directories in EFS format.
		 * XXX - Don't ship this option.
		 */
		fprintf(stderr,
			"WARNING: directories being dumped in EFS format\n");
		bsdformat = 0;
		break;
#endif
	case 'w':
		lastdump('w');		/* tell us only what has to be done */
		exit(0);
		break;
	case 'W':			/* what to do */
		lastdump('W');		/* tell us the current state of what has been done */
		exit(0);		/* do nothing else */
		break;

	case 'f':			/* output file */
		if(argc > 1) {
			argv++;
			argc--;
			tape = *argv;
		}
		break;

	case 'd':			/* density, in bits per inch */
		if (argc > 1) {
			argv++;
			argc--;
			density = atoi(*argv) / 10;
			if (density >= 625 && !bflag)
				ntrec = HIGHDENSITYTREC;
		}
		break;
	case 'C':	/* capacity in Kbytes (not necessarily same as TP_BSIZE!
			overrides density and/or length, if they are given */
		if (argc > 1) {
			char *nxt;
			argv++;
			argc--;
			capacity = strtoul(*argv, &nxt, NULL);
			if(!capacity)
				fprintf(stderr, "Need non-zero numeric argument with -C, ignored\n");
			else {
				if(*nxt == 'k' || *nxt == 'K')
					capacity *= 1024;
				else if(*nxt == 'm' || *nxt == 'M')
					capacity *= 1024*1024;
				Cflag = 1;
			}
		}
		else
			fprintf(stderr, "Need value with -C argument, ignored\n");
		break;

	case 's':			/* tape size, feet */
		if(argc > 1) {
			argv++;
			argc--;
			tsize = atol(*argv);
			tsize *= 12L*10L;
		}
		break;

	case 'b':			/* blocks per tape write */
		if(argc > 1) {
			argv++;
			argc--;
			bflag++;
			ntrec = atol(*argv);
		}
		break;

	case 'c':			/* Tape is cart. not 9-track */
		cartridge++;
		break;

	case '0':			/* dump level */
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		incno = arg[-1];
		break;

	case 'u':			/* update /etc/dumpdates */
		uflag++;
		break;

	case 'n':			/* notify operators */
		notify++;
		break;
	
	case 'F':
		/*
		 * For debugging, child process is not forked
		 */
		dofork = 0;
		break;

	default:
		fprintf(stderr, "dump: bad key '%c%'\n", arg[-1]);
		usage();
		Exit(X_ABORT);
	}
nooptions:
	if(argc > 1) {
		argv++;
		argc--;
		disk = *argv;
		orig_disk = disk;
	}
	if (disk == NULL) {
		msg("missing device name\n");
		exit(1);
	}
	if (strcmp(tape, "-") == 0) {
		pipeout++;
		tape = "standard output";
	}

	/*
	 * Determine how to default tape size and density (bytes per inch)
	 *
	 *         	density				tape size
	 * 9-track	1600 bpi (160 bytes/.1")	2300 ft. (2400  - slop)
	 * 9-track	6250 bpi (625 bytes/.1")	2300 ft. (2400  - slop)
 	 * cartridge	1000 bpi (100 bytes/.1")	5300 ft. (600*9 - slop)
	 * and many other kludges for 8mm, dat, etc.  The new C argument
	 * manages to avoid all of this.
	 */
	if (density == 0)
		density = cartridge ? 100 : 160;
	if (tsize == 0)
 		tsize = cartridge ? 5300L*120L : 2300L*120L;

	if ((arg = index(tape, ':')) != NULL) {
		*arg++ = 0;
		host = tape;
		tape = arg;
		if (rmthost(&host) == 0) {
			fprintf(stderr,
				"  DUMP: Couldn't execute %s on %s\n",
				"/etc/rmt", host);
			exit(X_ABORT);
		}
	}
	setuid(getuid());	/* rmthost() is the only reason to be setuid */
	if (signal(SIGHUP, sighup) == SIG_IGN)
		signal(SIGHUP, SIG_IGN);
	if (signal(SIGTRAP, sigtrap) == SIG_IGN)
		signal(SIGTRAP, SIG_IGN);
	if (signal(SIGFPE, sigfpe) == SIG_IGN)
		signal(SIGFPE, SIG_IGN);
	if (signal(SIGBUS, sigbus) == SIG_IGN)
		signal(SIGBUS, SIG_IGN);
	if (signal(SIGSEGV, sigsegv) == SIG_IGN)
		signal(SIGSEGV, SIG_IGN);
	if (signal(SIGTERM, sigterm) == SIG_IGN)
		signal(SIGTERM, SIG_IGN);
	

	if (signal(SIGINT, interrupt) == SIG_IGN)
		signal(SIGINT, SIG_IGN);

	set_operators();	/* /etc/group snarfed */
	getfstab();		/* /etc/fstab snarfed */
	/*
	 *	disk can be either the full special file name,
	 *	the suffix of the special file name,
	 *	the special name missing the leading '/',
	 *	the file system name with or without the leading '/'.
	 */
	dt = fstabsearch(disk);
	if (dt != 0)
		disk = rawname(dt);
rawopen:
	if (((fi = open(disk, 0)) < 0) || (stat(disk, &statb) < 0)) {
		msg("Cannot open/stat %s, %s\n", disk, strerror(errno));
		Exit(X_ABORT);
	}
	if ((statb.st_mode & S_IFMT) != S_IFCHR) {
		/*
		 * Try last ditch effort to find character device
		 */
		(void) close(fi);
		if (tryagain && ((disk = findrawpath(disk)) != NULL)) {
			tryagain = 0;
			goto rawopen;
		}
		msg("Cannot find character device for %s\n", orig_disk);
		Exit(X_ABORT);
	}
	if (dt != 0) {
		strncpy(spcl.c_dev, dt->mnt_fsname, NAMELEN);
		strncpy(spcl.c_filesys, dt->mnt_dir, NAMELEN);
	} else {
		strncpy(spcl.c_dev, disk, NAMELEN);
		strncpy(spcl.c_filesys, "an unlisted file system", NAMELEN);
	}


	strcpy(spcl.c_label, "none");
	gethostname(spcl.c_host, NAMELEN);
	spcl.c_host[NAMELEN-1] = '\0';
	spcl.c_level = incno - '0';
	spcl.c_type = TS_TAPE;

	/*
	 * There are no holes in EFS files,
	 * so we may as well do this only once.
	 * A possible improvement would be to check for all-zero
	 * tape blocks, and pretend they're holes.
	 */
	memset(spcl.c_addr, 1, sizeof spcl.c_addr);

	getitime();		/* /etc/dumpdates snarfed */

	msg("Date of this level %c dump: %s\n", incno, prdate(spcl.c_date));
 	msg("Date of last level %c dump: %s\n",
		lastincno, prdate(spcl.c_ddate));
	msg("Dumping %s ", disk);
	if (dt != 0)
		msgtail("(%s) ", dt->mnt_dir);
	if (host)
		msgtail("to %s on host %s\n", tape, host);
	else
		msgtail("to %s\n", tape);

	esize = 0;
	{
		static union { char buf[NBPSCTR]; struct efs fs; } un;
		sblock = &un.fs;
	}
	sync();
	bread((unsigned)EFS_SUPERBB, (char *)sblock, NBPSCTR);
	if (!IS_EFS_MAGIC(sblock->fs_magic)) {
		msg("bad magic number (0x%x) in superblock\n",
			sblock->fs_magic);
		msg("NOTE: may be an XFS filesystem - use xfsdump(1M)\n" );
		dumpabort();
	}
	EFS_SETUP_SUPERB(sblock);
	msiz = roundup(howmany(sblock->fs_ipcg * sblock->fs_ncg, BITSPERBYTE),
		TP_BSIZE);
	/*
	 * clrmap - bitmap of all inodes in use (di_mode & IFMT != 0)
	 * dirmap - bitmap of directories to be dumped
	 * nodmap - bitmap of non-directories to be dumped 
	 */
	clrmap = (char *)calloc(msiz, sizeof(char));
	dirmap = (char *)calloc(msiz, sizeof(char));
	nodmap = (char *)calloc(msiz, sizeof(char));

	anydskipped = 0;
	msg("mapping (Pass I) [regular files]\n");
	pass(mark, (char *)NULL);		/* mark updates esize */

	if (anydskipped) {
		do {
			msg("mapping (Pass II) [directories]\n");
			nadded = 0;
			pass(add, dirmap);
		} while(nadded);
	} else				/* keep the operators happy */
		msg("mapping (Pass II) [directories]\n");

	bmapest(clrmap);
	bmapest(nodmap);

	/* if not set explictly, calc capacity in kbytes from density, len, etc.  */
	if(Cflag) {
		fetapes = ((float)esize * TP_BSIZE) / ((float)capacity * 0x400);
		tsize = (long)capacity;	/* for reel changes in dumptape.c */
	}
	else {
		float tcap, delta;	/* delta is float to avoid 32 bit overflow */
		capacity = (ulong)(((float)density * (float)tsize) / 0x400);

		/* Estimate number of tapes, assuming streaming stops at
		   the end of each block written, and not in mid-block.
		   Assume no erroneous blocks; this can be compensated for
		   with an artificially low tape size.
		   Lost capacity in kbytes (over all of the tapes) is:
		( irg (.1" units) * density (bytes/.1") * #ofstops ) / 0x400
		   we then ignore the lost capacity; calculate the number
		   of tapes, and spread the delta over each tape.  Pretty
		   gruesome, and not correct for some kinds of tape drives.
		   We iterate twice on the delta to get reasonably close.
		   (agrees to about .5% with the pre-C version of dump
		   in the estimates except for the 8mm, where the old code
		   gave 1/2 the capacity it should with "0sd 6000 54000",
		   this code gives the "correct" number).
		*/
		if (cartridge)
			delta = ((15.48*density)*((float)esize/ntrec));
		else {
			int irg = (density == 625) ? 3 : 7;
			delta = (((float)irg*density)*((float)esize/ntrec));
		}
		tcap = (float)capacity * 0x400;
		fetapes = ((float)esize * TP_BSIZE) / tcap;
		if(fetapes >= 1.0)
			tcap -= delta / fetapes;
		else
			tcap -= delta;
		fetapes = ((float)esize * TP_BSIZE) / tcap;
		if(fetapes >= 1.0)
			delta /= fetapes * 0x400;
		else
			delta /= 0x400;
		capacity -= delta;
		fetapes = ((float)esize * TP_BSIZE) / ((float)capacity * 0x400);
	}

	etapes = fetapes + 1;		/* truncating assignment */
	/* count the nodemap on each additional tape */
	for (i = 1; i < etapes; i++)
		bmapest(nodmap);
	esize += i + 10;	/* headers + 10 trailer blocks */
	msg("estimated %ld tape blocks on %3.2f tape(s).\n", esize, fetapes);

	(void) alloctape();		/* Allocate tape buffer */

	otape();			/* bitmap is the first to tape write */
	time(&(tstart_writing));
	bitmap(clrmap, TS_CLRI);

	msg("dumping (Pass III) [directories]\n");
	dirphase = 1;
 	pass(dirdump, dirmap);
	dirphase = 0;

	msg("dumping (Pass IV) [regular files]\n");
	pass(dump, nodmap);

	spcl.c_type = TS_END;
	if (host == NULL)
		tflush();

	putitime();
	if (host == NULL) {
		if (!pipeout) {
			if (close(to) < 0) {
				msg("Error closing tape, %s\n",strerror(errno));
				dumpabort();
			}
			to = -1;
			dump_rewind();
		}
	} else {
		tflush();
		dump_rewind();
	}
	msg("DUMP: %ld tape blocks on %d tape(s)\n",spcl.c_tapea,spcl.c_volume);
	msg("DUMP IS DONE\n");
	broadcast("DUMP IS DONE!\7\7\n");
	Exit(X_FINOK);
	/*NOTREACHED*/
}

static void
usage(void)
{
	fprintf(stderr,
	"Usage: dump [ 0123456789fusdbcCWwn [argument ...] ] filesystem\n");
}

static void
sighup()
{
	msg("SIGHUP()  try rewriting\n");
	sigAbort();
}

static void
sigtrap()
{
	msg("SIGTRAP()  try rewriting\n");
	sigAbort();
}

static void
sigfpe()
{
	msg("SIGFPE()  try rewriting\n");
	sigAbort();
}

static void
sigbus()
{
	msg("SIGBUS()  try rewriting\n");
	sigAbort();
}

static void
sigsegv()
{
	msg("SIGSEGV()  ABORTING!\n");
	abort();
}

void
sigalrm()
{
	msg("SIGALRM()  try rewriting\n");
	sigAbort();
}

static void
sigterm()
{
	msg("SIGTERM()  try rewriting\n");
	sigAbort();
}

static void
sigAbort(void)
{
	if (pipeout) {
		msg("Unknown signal, cannot recover\n");
		dumpabort();
	}
	msg("Rewriting attempted as response to unknown signal.\n");
	fflush(stderr);
	fflush(stdout);
	close_rewind();
	exit(X_REWRITE);
}

char *
rawname(struct mntent *mnt)
{
	char *cp;
	static char buf[128];
	char *sep = NULL;

	if (mnt->mnt_opts != NULL && (cp = hasmntopt(mnt, MNTOPT_RAW)) != NULL) {
		cp += strlen(MNTOPT_RAW)+1;
		if (sep = strchr(cp, ','))
			*sep = '\0';
	} else
		cp = mnt->mnt_fsname;
	strcpy(buf, cp);
	if (sep)
		*sep = ',';
	return buf;
}
