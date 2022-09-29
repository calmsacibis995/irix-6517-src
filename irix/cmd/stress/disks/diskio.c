/*
 * Copyright (c) Silicon Graphics, Inc.  1987
 * All Rights Reserved.
 *
 *	diskio benchmark - J. M. Barton   12/10/87
 *
 *	-n - set the number of parallel processes for banging
 *	-r - set the number of times that a file is scanned
 *	-s - set the file size for scanning
 *	-a - align the transfers on a natural boundary
 *	-d - destructive (write instead of read)
 *	-b - write AND read the files with VERIFY
 *      -p - raw read/write on a disk partition
 *      -v - verbose mode
 *	-1 - run phase 1 only
 *	-2 - run phase 2 only
 *
 *	remaining arguments assumed to specify the block size(s) to use,
 *	which overrides the default sizes specified in the program.
 *
 *   Phase 1 Philosophy:
 *      This program works by creating a number of children, each of
 *   which scans a file, either reading or writing, a fixed number
 *   of times, using a small number of different block sizes.  When
 *   running this, the size of the files and the number of processes
 *   should be adjusted to blow the buffer cache - else the numbers
 *   are meaningless.  You can modify the block sizes (see the code)
 *   to suit personal preferences, or to add other sizes to check.
 *
 *      The throughput numbers are exact counts versus clock ticks, and
 *   so accurately measure the throughput among all processes working
 *   on a block size.  The processes are synchronized, such that they
 *   all work on the same block size at a time.  The files are scanned
 *   sequentially, thus insuring worst case behaviour of the buffer cache
 *   in all cases.
 *
 *      The final number is reported as aggragate throughput, which is
 *   simply the total number of bytes transferred by all processes divided
 *   by the real time passed in each process.  In my mind, this is the
 *   most interesting number, since it provides an average throughput
 *   that can be expected in a real job mix.
 *
 *   Phase 2 Philosophy:
 *      This phase is very similar to phase 1, except that the different
 *   blocksizes are in parallel, simulating a more real load (maybe).
 *   The blocksizes are run by simply rotoring each process through the
 *   blocksizes starting with a different element for each one.
 *
 *   Compiling:
 *      This program should compile on most UNIX variants.  Define
 *   BSD4_2 for BSD4.2 or 4.3 systems, do nothing for most others.
 *   If your system is version 7, System III or earlier, define
 *   NOSEMS to use pipes for synchronization instead of semaphores.
 *   The NOSEMS define is turned on automatically if BSD4_2 is on.
 */

# include 	<stdio.h>
# include	<fcntl.h>
# include	<memory.h>
# include	<signal.h>
# include	<stdarg.h>
# include	<stdlib.h>
# include	<string.h>
# include	<unistd.h>
# include	<sys/types.h>
# include	<sys/param.h>
# ifdef BSD4_2
#	define	NOSEMS
#	define	MICPER		1000000
#	include	<sys/time.h>
#	include	<sys/resource.h>
# else
# 	include	<sys/times.h>
# 	include	<sys/ipc.h>
# 	include	<sys/sem.h>
# endif

# define	PALIGN		(~0x3ffL)
# define	BIGMINVAL	0x8000000

typedef struct {
	int		blocknum;	/* step number */
	long		onemoved;	/* actual bytes moved */
	long		totmoved;	/* total bytes moved */
# ifdef BSD4_2
	struct timeval	minticks;
	struct rusage	vminticks;
	struct timeval	maxticks;
	struct rusage	vmaxticks;
	struct timeval	totticks;
	struct rusage	vtotticks;
# else
	long		minticks;	/* minimum time seen */
	struct tms	vminticks;	/* process relative minimum */
	long		maxticks;	/* maximum time seen */
	struct tms	vmaxticks;	/* process relative maximum */
	long		totticks;	/* total tick time seen */
	struct tms	vtotticks;	/* process relative total */
# endif
} counter_t;

/*
 * Forward declarations of local functions
 */
static long bangandverify(int, long, long, unsigned int, int);
static void cleanup(void);
static void eyetoy(int);
static int  makefile(int, long);
static void printrate(long, int, int);
static long rawbangit(int, long, long, unsigned int, int, int);
static void rawbangrange1(int, long, long, int, int, int);
static void rawbangrange2(int, long, long, int, int, int);
static long readpattern(int, unsigned int, long, long);
static void runfile(long, int, int);
static void runraw(char *, long, int, int);
static void sumone(counter_t *);
static void startem(int);
static void waitnext(void);
static long writepattern(int, unsigned int, long, long);
static void vperror(char *fmt, ...);

/*
 * Macros for adding, subtracting and testing time and tick counter
 * values.  These are used to reduce changes to the code for BSD versus
 * System V differences.
 *
 *	addtime(t1, t2) 	- add the value of t2 to t1
 *	difftime(t1, t2)	- place t2-t1 in t1
 *	zerotime(t1)		- set t1 to zero
 *	testtime(t1,t2)		- >0 if t1>t2, 0 otherwise
 *	testzero(t1)		- nonzero if time t1 is 0
 *	timetosec(t1)		- convert the given time to seconds
 *	addtms(t1, t2)		- add the CPU usage t2 to t1
 *	difftms(t1, t2)		- subtract the CPU usage (t1 = t2-t1)
 *	zerotms(t1)		- set the CPU usage to zero
 */

# ifdef BSD4_2

paddtime(t1, t2)
   struct timeval	*t1;
   struct timeval	*t2;
{
	t1->tv_sec += t2->tv_sec;
	t1->tv_usec += t2->tv_usec;
	if (t1->tv_usec >= MICPER) {
		t1->tv_sec++;
		t1->tv_usec -= MICPER;
	}
}

# define		addtime(t1, t2) 	paddtime(&t1, &t2)

pdifftime(t1, t2)
   struct timeval	*t1;
   struct timeval	*t2;
{
	t1->tv_sec = t2->tv_sec - t1->tv_sec;
	if (t1->tv_usec >= t2->tv_usec) {
		t1->tv_sec--;
		t1->tv_usec = MICPER - (t1->tv_usec - t2->tv_usec);
	}
	else
		t1->tv_usec = t2->tv_usec - t1->tv_usec;
}

# define		difftime(t1, t2)	pdifftime(&t1, &t2)

# define		zerotime(t1)	\
	{t1.tv_sec = 0; t1.tv_usec = 0; }

# define		testtime(t1, t2) \
	((t1.tv_sec > t2.tv_sec)?1:(t1.tv_sec < t2.tv_sec)?0: \
	 (t1.tv_usec > t2.tv_usec)?1:0)

# define		testzero(t1)	(t1.tv_sec == 0 && t1.tv_usec == 0)

# define		timetosec(t1)	t1.tv_sec

# define		addtms(t1, t2) \
	{addtime(t1.ru_utime, t2.ru_utime);	\
	 addtime(t1.ru_stime, t2.ru_utime);}

# define		difftms(t1, t2) \
	{difftime(t1.ru_utime, t2.ru_utime);	\
	 difftime(t1.ru_stime, t2.ru_stime);}

# define		zerotms(t1)	\
	(t1.ru_utime.tv_sec = 0, t1.ru_utime.tv_usec = 0, \
	 t1.ru_stime.tv_sec = 0, t1.ru_stime.tv_usec = 0)

# define	tms_utime	ru_utime
# define	tms_stime	ru_stime

# else	/* BSD4_2 */

# define		addtime(t1,t2) 		t1 += t2

# define		difftime(t1, t2)	t1 = t2 - t1

# define		zerotime(t1)		t1 = 0

# define		testtime(t1, t2)	(t1 > t2)

# define		testzero(t1)		(t1 == 0)

# define		timetosec(t1)		((t1)/(double)HZ)

# define		addtms(t1,t2)	\
	(t1.tms_utime += t2.tms_utime, t1.tms_stime += t2.tms_stime)

# define		difftms(t1, t2)	\
	{difftime(t1.tms_utime, t2.tms_utime); \
	 difftime(t1.tms_stime, t2.tms_stime);}

# define		zerotms(t1)	\
	(t1.tms_utime = 0, t1.tms_stime = 0)

# endif	/* BSD4_2 */

/*
 * Global variables and constants of the program.
 */
char		*pname;
# ifdef BSD4_2
struct timeval		stimev;
struct rusage		stms;
# else
long		stimev;
struct tms	stms;
# endif
int		nproc = 5;
long		fsize = (350 * 1024);
int		repeats = 10;
int		rw = 0;
int		align = 0;
int		askphase = 0;
int		phase;
int		procnum;
int		semdes;
int		verbose = 0;
char		*bblk;

# ifdef NOSEMS
int		readsync;	/* read end of sync pipe */
int		writesync;	/* write end of sync pipe */
# endif

/*
 * The blocksizes we will try.  This is a good place to tune for
 * a representative benchmark.
 */
# define	NDEFSTEPS	8
unsigned int	defblocksizes[NDEFSTEPS] = {
	512,
	1024,
	2048,
	4096,
	8192,
	(16*1024),
	(24*1024),
	(32*1024)
};
counter_t	cntdefault[NDEFSTEPS];
counter_t	*counters = cntdefault;
int		NSTEPS = NDEFSTEPS;
unsigned int	*blocksizes = defblocksizes;

int
main(int argc, char **argv)
{
   extern int	optind;
   extern char	*optarg;
   int		c;
   int		err = 0;
   int		i;
   unsigned int bsize;
   char		*part = 0;
   long		nsize;

	/*
	 * Parse the arguments.
	 */
	pname = argv[0];
	while ((c = getopt(argc, argv, "v12p:dban:s:r:")) != EOF) {
		switch (c) {
		case '1':
			askphase = 1;
			break;
		case '2':
			askphase = 2;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'p':
			part = optarg;
			break;
		case 'a':
			align = PALIGN;
			break;
		case 'n':
			if ((nproc = (int)strtol(optarg, (char **) 0, 0)) <= 0) {
				fprintf(stderr,
					"%s: (n option) must be > 0\n", pname);
				err++;
			}
			break;
		case 's':
			if ((fsize = strtol(optarg, (char **) 0, 0)) <= 1024) {
				fprintf(stderr,
					"%s: (s option) must be >= 1024\n",
					pname);
				err++;
			}
			if ((nsize = (fsize / (long)blocksizes[NSTEPS-1]) *
				(long)blocksizes[NSTEPS-1]) != fsize) {
				if (nsize == 0) {
					fprintf(stderr,
			"%s: size not large enough for biggest blocksize\n",
						pname);
					err++;
				}
				else {
					if (verbose)
						fprintf(stderr,
					"%s: size rounded down to %#lx bytes\n",
						pname, nsize);
				}
				fsize = nsize;
			}
			break;
		case 'r':
			if ((repeats = (int)strtol(optarg, (char **) 0, 0))
				<= 0) {
				fprintf(stderr, "%s: (r option) must be > 0\n",
					pname);
				err++;
			}
			break;
		case 'd':
			rw = 1;
			break;
		case 'b':
			rw = 2;
			break;
		case '?':
			err++;
			break;
		}
	}
	if (err) {
		fprintf(stderr, "usage: %s\n", pname);
		exit(1);
	}
	/*
	 * If user specified the blocksizes to use, set up the list.
	 */
	if (optind < argc) {
		NSTEPS = argc - optind;
		if ((counters = (counter_t *) calloc(NSTEPS,
			sizeof(counter_t))) == 0) {
			vperror("%s: out of memory:", pname);
			exit(1);
		}
		if ((blocksizes = (unsigned int *) calloc(NSTEPS,
			sizeof(unsigned int))) == 0) {
			vperror("%s: out of memory:", pname);
			exit(1);
		}
		for (i = 0; optind < argc; optind++, i++) {
			blocksizes[i] =
			   (unsigned int) strtoul(argv[optind], (char **) 0, 0);
			if (blocksizes[i] == 0) {
				fprintf(stderr, "%s: invalid block size %s\n",
					pname, argv[optind]);
				exit(1);
			}
		}
	}
	/*
	 * Handle aligned vs. non-aligned transfers.
	 */
	if (align) {
		if ((bblk = malloc(blocksizes[NSTEPS-1]*2)) == 0) {
			fprintf(stderr, "%s: out of memory\n", pname);
			exit(1);
		}
		bblk = (char *)(((unsigned long)bblk +
			(blocksizes[NSTEPS-1] - 1)) & (unsigned long)align);
		if (verbose)
			printf("%s: using page aligned buffer at %#lx\n",
				pname, bblk);
	}
	else {
		bblk = 0;
		bsize = blocksizes[NSTEPS-1];
		do {
			if (bblk != 0)
				free(bblk);
			if ((bblk = malloc(bsize)) == 0) {
				fprintf(stderr, "%s: out of memory\n", pname);
				exit(1);
			}
			bsize += 31;
		} while (!((unsigned long) bblk & (unsigned long) PALIGN));
		if (verbose)
			printf("%s: using non-page-aligned buffer at %#lx\n",
				pname, bblk);
	}
	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);
	/*
	 * Set up the synchronization handle for controlling the
	 * children.
	 */
# ifdef NOSEMS
	{
	   int	lpif[2];

		if (pipe(lpif) == -1) {
			vperror("%s: sync pipe", pname);
			exit(1);
		}
		writesync = lpif[1];
		readsync = lpif[0];
	}
# else
	if ((semdes = semget(IPC_PRIVATE, 1, IPC_CREAT|0600)) == -1) {
		vperror("%s: semget", pname);
		exit(1);
	}
# endif

	/*
	 * Perform phase 1 if wanted - handle one block size at a time.
	 */
	if (askphase == 0 || askphase == 1) {
		if (verbose)
			printf("%s: phase 1 begins - blocks in sync\n", pname);
		phase = 1;
# ifndef NOSEMS
		semctl(semdes, 0, SETVAL, 0);
# endif
		if (!part)
			runfile(fsize, repeats, rw);
		else
			runraw(part, fsize, repeats, rw);
		printrate(fsize, repeats, rw);
	}

	/*
	 * Perform phase 2 if wanted - unordered blocksizes.
	 */
	if ((askphase == 0 || askphase == 2) && NSTEPS > 1 && nproc > 1) {
		if (verbose)
			printf("%s: phase 2 begins - block sizes at random\n",
				pname);
		phase = 2;
# ifndef NOSEMS
		semctl(semdes, 0, SETVAL, 0);
# endif
		if (!part)
			runfile(fsize, repeats, rw);
		else
			runraw(part, fsize, repeats, rw);
		printrate(fsize, repeats, rw);
	}

# ifndef NOSEMS
	semctl(semdes, 0, IPC_RMID, 0);
# endif
	exit(0);
	/* NOTREACHED */
}

static void
vperror(char *fmt, ...)
{
   va_list	ap;
   char		vpbuf[BUFSIZ];

	va_start(fmt, ap);

# ifdef NOSEMS
	sprintf(vpbuf, fmt, ap);
# else
	vsprintf(vpbuf, fmt, ap);
# endif
	perror(vpbuf);

	va_end(ap);
}

static void
cleanup(void)
{
# ifndef NOSEMS
	semctl(semdes, 0, IPC_RMID, 0);
# endif
	exit(1);
}

/*
 * Start the timer going.
 */
static void
starttimer(void)
{
# ifdef BSD4_2
   struct timezone	tzb;

	getrusage(RUSAGE_SELF, &stms);
	gettimeofday(&stimev, &tzb);
# else
	stimev = times(&stms);
# endif
}

/*
 * Stop the timer and collect the results.
 */
static void
stoptimer(void)
{
# ifdef BSD4_2
   struct timezone	tzb;
   struct timeval	etime;
   struct rusage	etms;

	gettimeofday(&etime, &tzb);
	getrusage(RUSAGE_SELF, &etms);
# else
   long		etime;
   struct tms	etms;

	etime = times(&etms);
# endif
	difftime(stimev, etime);
	difftms(stms, etms);
}

/*
 * Openraw - open the raw device specified and make sure it can
 *   hold all the data.
 */
static int
openraw(char *dev)
{
   int		fd;

	if ((fd = open(dev, O_RDWR)) == -1) {
		vperror("%s: raw device open", pname);
		exit(1);
	}
	lseek(fd, 0L, 0);
	return(fd);
}

/*
 * Makeraw - make a raw file for the test.
 */
static long
makeraw(int fd, int p, long fs)
{
   long		off;

	off = p * fs;
	if (lseek(fd, off, 0) == -1) {
		vperror("%s: seek on raw device", pname);
		exit(1);
	}
	return(off);
}

/*
 * Make a file of the given characteristics.
 */
static int
makefile(
   int		seq,		/* file sequence number */
   long		size)		/* size to make the file */
{
   char		buf[BUFSIZ];
   char		fn[20];
   int		fd;
   int		cnt;

	sprintf(fn, "diskio%04d", seq);
	if ((fd = open(fn, O_RDWR|O_CREAT|O_TRUNC, 0600)) == -1) {
		vperror("tmp file %s", fn);
		exit(1);
	}
	memset(buf, 0xa5, BUFSIZ);
	while (size > 0) {
		cnt = (int)(size > BUFSIZ ? BUFSIZ : size);
		if (write(fd, buf, cnt) == -1) {
			vperror("tmp file %s", fn);
			close(fd);
			unlink(fn);
			exit(1);
		}
		size -= cnt;
	}
	lseek(fd, 0L, 0);
	unlink(fn);
	return(fd);
}

/*
 * One instance of a speed test.  Takes a file descriptor, the file
 * is assumed to be "right" already.
 */

static long
rawbangit(
   int		fd,	/* file descriptor */
   long		off,	/* offset into file */
   long		fsize,	/* file size */
   unsigned int	fblock,	/* block size */
   int		tries,	/* number of passes to make */
   int		rw)	/* read/write/both flag */
{
   int		i;
   long		nbytes;
   int		bothtoggle;
   long		totbytes;

	if (rw == 2)
		return(bangandverify(fd, off, fsize, fblock, tries));
	bothtoggle = 0;
	totbytes = 0;
	for (i = 0; i < tries; i++) {
		lseek(fd, off, 0);
		nbytes = 0;
		while (nbytes <= fsize - fblock) {
			switch (rw) {
			case 1:
			writeit:
				if (write(fd, bblk, fblock) == -1) {
					vperror("%s: bangit write", pname);
					exit(1);
				}
				break;
			case 0:
			readit:
				if (read(fd, bblk, fblock) == -1) {
					vperror("%s: bangit read", pname);
					exit(1);
				}
				break;
			case 3:
				if (bothtoggle++)
					goto writeit;
				else
					goto readit;
			}
			nbytes += fblock;
			totbytes += fblock;
		}
		if (~(bothtoggle & 01))
			bothtoggle++;
	}
	return(totbytes);
}

/*
 * bangandverify - read/write the disk and verify the data.
 */
static long
bangandverify(
   int		fd,
   long		off,	/* offset into file */
   long		fsize,	/* file size */
   unsigned int	fblock,	/* block size */
   int		tries)	/* number of passes to make */
{
   int		i;
   long		nbytes;
   long		totbytes;

	totbytes = 0;
	for (i = 0; i < tries; i++) {
		lseek(fd, off, 0);
		if (i & 01) {
			writepattern(fd, fblock, fsize, 0xa5a5a5a5);
			lseek(fd, off, 0);
			totbytes += readpattern(fd, fblock, fsize, 0xa5a5a5a5);
		}
		else {
			writepattern(fd, fblock, fsize, 0x5a5a5a5a);
			lseek(fd, off, 0);
			totbytes += readpattern(fd, fblock, fsize, 0x5a5a5a5a);
		}
	}
	return(totbytes);
}

static long
writepattern(int fd, unsigned int fblock, long flen, long pat)
{
   unsigned int	id;
   unsigned int wl;
   long		nbytes;

	wl = fblock / (unsigned int)sizeof(long);
	for (id = 0; id < wl; id++)
		*(((long *) bblk) + id) = pat;
	for (nbytes = 0; nbytes <= flen - fblock; nbytes += fblock)
		if (write(fd, bblk, fblock) == -1) {
			vperror("%s: pattern write", pname);
			exit(1);
		}
	return(nbytes);
}

static long
readpattern(int fd, unsigned int fblock, long flen, long pat)
{
   unsigned int	id;
   unsigned int wl;
   long		nbytes;

	wl = fblock / (unsigned int)sizeof(long);
	for (nbytes = 0; nbytes <= flen - fblock; nbytes += fblock) {
		if (read(fd, bblk, fblock) == -1) {
			vperror("%s: pattern read", pname);
			exit(1);
		}
		for (id = 0; id < wl; id++)
			if (*(((long *) bblk) + id) != pat) {
				printf("verify error at address 0x%x\n",
					bblk + (id * sizeof(long)));
				printf("program pausing ... (^C to end)\n");
				pause();
			}
	}
	return(nbytes);
}
					
/*
 * Run the speed test on a raw range of block sizes.
 */

static void
rawbangrange1(
    int		fd,		/* file descriptor */
    long	off,		/* offset into file */
    long	fsize,		/* file size */
    int		tries,		/* number of iterations */
    int		rw,		/* read/write flag */
    int		results)	/* write results to this descriptor */
{
    int		bcnt;
    long	onemoved;

	for (bcnt = 0; bcnt < NSTEPS; bcnt++) {
		counters[bcnt].totmoved = 0;
		zerotime(counters[bcnt].totticks);
		zerotms(counters[bcnt].vtotticks);
		counters[bcnt].blocknum = 0;
	}
	for (bcnt = 0; bcnt < NSTEPS; bcnt++) {
		waitnext();
		starttimer();
		onemoved = rawbangit(fd, off, fsize, blocksizes[bcnt],
			tries, rw);
		stoptimer();
		counters[bcnt].totmoved = onemoved;
		addtime(counters[bcnt].totticks, stimev);
		addtms(counters[bcnt].vtotticks, stms);
		counters[bcnt].blocknum = bcnt;
		write(results, &counters[bcnt], sizeof(counter_t));
	}
}

/*
 * Run the speed test on a raw range of block sizes.
 */

static void
rawbangrange2(
    int		fd,		/* file descriptor */
    long	off,		/* offset into file */
    long	fsize,		/* file size */
    int		tries,		/* number of iterations */
    int		rw,		/* read/write flag */
    int		results)	/* write results to this descriptor */
{
    int		bcnt;
    int		tcnt;
    long	onemoved;

	for (bcnt = 0; bcnt < NSTEPS; bcnt++) {
		counters[bcnt].totmoved = 0;
		zerotime(counters[bcnt].totticks);
		zerotms(counters[bcnt].vtotticks);
		counters[bcnt].blocknum = 0;
	}
	waitnext();
	for (tcnt = 0; tcnt < tries; tcnt++) {
		for (bcnt = procnum; bcnt < NSTEPS; bcnt++) {
			starttimer();
			onemoved = rawbangit(fd, off, fsize,
				blocksizes[bcnt], 1, rw);
			stoptimer();
			counters[bcnt].totmoved += onemoved;
			addtime(counters[bcnt].totticks, stimev);
			addtms(counters[bcnt].vtotticks, stms);
		}
		for (bcnt = 0; bcnt < procnum; bcnt++) {
			starttimer();
			onemoved = rawbangit(fd, off, fsize,
				blocksizes[bcnt], 1, rw);
			stoptimer();
			counters[bcnt].totmoved += onemoved;
			addtime(counters[bcnt].totticks, stimev);
			addtms(counters[bcnt].vtotticks, stms);
		}
	}
	for (bcnt = 0; bcnt < NSTEPS; bcnt++) {
		counters[bcnt].blocknum = bcnt;
		write(results, &counters[bcnt], sizeof(counter_t));
	}
}

/*
 * phase1collect - synchronize the processes involved and collect the
 *    data as they produce it.
 */

static void
phase1collect(int sink)
{
   int		pnum;
   int		pcnt;
   counter_t	*cp;
   counter_t	lcnt;

	eyetoy(NSTEPS * nproc);
	for (pnum = 0; pnum < NSTEPS; pnum++) {
		startem(nproc);
		for (pcnt = 0; pcnt < nproc; pcnt++) {
			if (read(sink, &lcnt, sizeof(counter_t)) > 0) {
				sumone(&lcnt);
				eyetoy(0);
			}
			else {
				fprintf(stderr, "%s: unexpected end of data\n",
					pname);
				exit(1);
			}
		}
	}
}

/*
 * phase2collect - run an unsychronized set of processes and collect the
 *    data as they produce it.
 */

static void
phase2collect(int sink)
{
   int		pnum;
   int		pcnt;
   counter_t	*cp;
   counter_t	lcnt;

	startem(nproc);
	eyetoy(NSTEPS * nproc);
	for (pnum = 0; pnum < NSTEPS; pnum++) {
		for (pcnt = 0; pcnt < nproc; pcnt++) {
			if (read(sink, &lcnt, sizeof(counter_t)) > 0) {
				sumone(&lcnt);
				eyetoy(0);
			}
			else {
				fprintf(stderr, "%s: unexpected end of data\n",
					pname);
				exit(1);
			}
		}
	}
}

/*
 * sumone - add the given new value into the counter given.
 */
static void
sumone(counter_t *lcnt)
{
   counter_t	*cp;

	cp = &counters[lcnt->blocknum];
	if (!testtime(lcnt->totticks, cp->minticks)) {
		cp->minticks = lcnt->totticks;
		cp->vminticks = lcnt->vtotticks;
	}
	if (testtime(lcnt->totticks, cp->maxticks)) {
		cp->maxticks = lcnt->totticks;
		cp->vmaxticks = lcnt->vtotticks;
	}
	addtime(cp->totticks, lcnt->totticks);
	cp->totmoved += lcnt->totmoved;
	addtms(cp->vtotticks, lcnt->vtotticks);
}

/*
 * Run a raw-disk oriented speed test
 */
static void
runraw(
   char		*dev,		/* name of the raw device */
   long		fsize,		/* size of the chunk */
   int		tries,		/* number of tries */
   int		rw)		/* read/write/both flag */
{
   int		pnum;
   int		proc;
   int		pif[2];
   long		off;
   int		rd;
   int		collect();
   char		c;

	memset((char *) counters, 0, sizeof(counter_t) * NSTEPS);
	for (pnum = 0; pnum < NSTEPS; pnum++)
		addtime(counters[pnum].minticks, BIGMINVAL);

	if (pipe(pif) == -1) {
		vperror("%s: runraw pipe", pname);
		exit(1);
	}
	if (verbose)
		printf("%s: [setting up ... ", pname);
	fflush(stdout);
	rd = openraw(dev);
	for (pnum = 0; pnum < nproc; pnum++) {
		if ((proc = fork()) == 0) {
			off = makeraw(rd, pnum, fsize);
			close(pif[0]);
			write(pif[1], &c, 1);
			if (phase == 2) {
				procnum = pnum;
				rawbangrange2(rd, off, fsize, tries,
					rw, pif[1]);
			}
			else {
				procnum = 0;
				rawbangrange1(rd, off, fsize, tries,
					rw, pif[1]);
			}
			close(pif[1]);
			close(rd);
			exit(0);
		}
		else if (proc == -1) {
			vperror("%s: fork child:", pname);
			exit(1);
		}
	}
	close(pif[1]);
	close(rd);
	for (pnum = 0; pnum < nproc; pnum++)
		read(pif[0], &c, 1);

	if (verbose)
		printf("running ... ");
	fflush(stdout);
	if (phase == 1)
		phase1collect(pif[0]);
	else
		phase2collect(pif[0]);
	close(pif[0]);
	if (verbose)
		printf("done]\n");
}

/*
 * Run a file-oriented speed test.
 */
static void
runfile(
   long		fsize,		/* file size to use */
   int		tries,		/* number of tries */
   int		rw)		/* read/write/both flag */
{
   int		pnum;
   int		proc;
   int		pif[2];
   int		fd;
   int		collect();
   char		c;


	memset((char *) counters, 0, sizeof(counter_t) * NSTEPS);
	for (pnum = 0; pnum < NSTEPS; pnum++)
		addtime(counters[pnum].minticks, BIGMINVAL);

	if (pipe(pif) == -1) {
		vperror("%s: runfile pipe", pname);
		exit(1);
	}
	if (verbose)
		printf("%s: [setting up ... ", pname);
	fflush(stdout);
	for (pnum = 0; pnum < nproc; pnum++) {
		if ((proc = fork()) == 0) {
			fd = makefile(pnum, fsize);
			close(pif[0]);
			write(pif[1], &c, 1);
			if (phase == 1) {
				procnum = 0;
				rawbangrange1(fd, 0L, fsize, tries, rw, pif[1]);
			}
			else {
				procnum = pnum;
				rawbangrange2(fd, 0L, fsize, tries, rw, pif[1]);
			}
			close(pif[1]);
			close(fd);
			exit(0);
		}
		else if (proc == -1) {
			vperror("%s: fork child:", pname);
			exit(1);
		}
	}
	close(pif[1]);
	for (pnum = 0; pnum < nproc; pnum++)
		read(pif[0], &c, 1);
	if (verbose)
		printf("running ... ");
	fflush(stdout);
	if (phase == 1)
		phase1collect(pif[0]);
	else
		phase2collect(pif[0]);
	close(pif[0]);
	if (verbose)
		printf("done]\n");
}

/*
 * Print the rates out in an interesting fashion.
 */
static void
printrate(long fsize, int tries, int rw)
{
   int		bcnt;
   counter_t	*cp;
   long		totbytes;
   double	avg;
   double	max;
# ifdef BSD4_2
   struct timeval	totticks;
# else
   long			totticks;
# endif

	if (verbose) {
		printf("%s: phase %d action %s max span %ld loops %d\n",
			pname, phase, (rw == 0 ? "read" : rw == 1 ? "write" :
			"read/write"), fsize, tries);
		printf("  chunk   min    max    avg    dsort  user    sys   avgr\n");
	}
	zerotime(totticks);
	totbytes = 0;
	for (bcnt = 0; bcnt < NSTEPS; bcnt++) {
		cp = &counters[bcnt];
		printf("  %5u", blocksizes[bcnt]);
		if (verbose) {
			if (!testzero(cp->minticks))
				printf(" %6.2f",
					timetosec((float)cp->minticks));
			else
				printf(" %6s", "nomin");
			printf(" %6.2f", (max =
				timetosec((float)cp->maxticks)));
			printf(" %6.2f", (avg = timetosec((float)cp->totticks)
				/ (float)nproc));
			printf(" %6.2f", (max - avg) / max);
			printf(" %6.2f", timetosec(cp->vtotticks.tms_utime));
			printf(" %6.2f", timetosec(cp->vtotticks.tms_stime));
		}
		if (verbose)
			printf(" ");
		else
			printf("\t");
		if (testzero(cp->totticks))
			printf("%6.1f", 0.0);
		else
			printf("%6.1f",
				((float)cp->totmoved /
				 timetosec((float)cp->totticks)) / 1000.0);
		printf("\n");
		totbytes += cp->totmoved;
		addtime(totticks, cp->totticks);
	}
	if (NSTEPS > 1) {
		if (verbose)
			printf(" aggragate throughput %6.1f Kbytes/sec\n", 
				((float)totbytes / timetosec((float)totticks))
				/ 1000.0);
		else
			printf("  overall\t%6.1f\n",
				((float)totbytes / timetosec((float)totticks))
				/ 1000.0);
	}
}

/*
 * Wait for the semaphore to be set for us to go on.
 */
static void
waitnext(void)
{
# ifdef NOSEMS
   char			c;

	if (read(readsync, &c, 1) == -1) {
		vperror("%s: readsync", pname);
		exit(1);
	}
# else
   struct sembuf	sops[1];

	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[0].sem_flg = 0;
	if (semop(semdes, sops, 1) == -1) {
		vperror("%s: semop", pname);
		exit(1);
	}
# endif
}

/*
 * Start all process waiting to go.
 */
static void
startem(int n)
{
# ifdef NOSEMS
   char			sbuf[100];

	if (write(writesync, sbuf, n) == -1) {
		vperror("%s: writesync", pname);
		exit(1);
	}
# else
   struct sembuf	sops[1];

	sops[0].sem_num = 0;
	sops[0].sem_op = (short)n;
	sops[0].sem_flg = 0;
	if (semop(semdes, sops, 1) == -1) {
		vperror("%s: semop", pname);
		exit(1);
	}
# endif
}

/*
 * Nifty little toy for interactive uses - moves a cursor to indicate
 * that things are actually happening.
 */
# define		EYEBUF		30

static void
eyetoy(int mode)
{
   static int	atall = 1;
   static int	cnt;
   static char	backup[EYEBUF];
   char		buf[EYEBUF];
   int		len;
   int		i;

	if (!verbose)
		return;

	switch (mode) {
	default:
		if (!atall || !isatty(1)) {
			atall = 0;
			return;
		}
		fflush(stdout);
		for (i = 0; i < EYEBUF; i++)
			backup[i] = '';
		cnt = 0;
		sprintf(buf, "%5d(%5d)", 0, mode);
		len = (int)strlen(buf);
		write(1, buf, len);
		write(1, backup, len);
		break;
	case 0:
		if (!atall)
			return;
		cnt++;
		sprintf(buf, "%5d", cnt);
		len = (int)strlen(buf);
		write(1, buf, len);
		write(1, backup, len);
		break;
	}
}
