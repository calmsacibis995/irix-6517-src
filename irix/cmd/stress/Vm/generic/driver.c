# include <sys/types.h>
# include <stdio.h>
# include <errno.h>
# include <signal.h>
# include <fcntl.h>
# include "norm.h"
# include <sys/immu.h>
# include <sys/prctl.h>
# include <sys/mman.h>
# include <task.h>
# include <stdlib.h>
# include <unistd.h>

#ident "$Revision: 1.10 $"

static int pagesize;
#undef _PAGESZ
#define _PAGESZ		pagesize

void	norm();
void	dmphist();
double	atof();
void	slave();
void	sigc();
void	dumphist();

long size, locality, center;
int histograms;
int *base;

struct nums {
	int	hist;
	long	chkval;
};
struct nums *tbase;
double scale;

void
usage(name)
char	*name;
{
	fprintf(stderr, "Usage: %s -s size [-c cycles -n nprocs -l locality -b burpn -r readratio -w wander -a -S sleep -h -m]\n", name);
	fprintf(stderr, "	-s size		size in bytes of data space\n");
	fprintf(stderr, "	-c cycles	number of mem references\n");
	fprintf(stderr, "	-l locality	scale of locality (float)\n");
	fprintf(stderr, "	-r readratio	reads to writes (int)\n");
	fprintf(stderr, "	-b burpn	make noise after each burpn references\n");
	fprintf(stderr, "	-n nprocs	number of processes\n");
	fprintf(stderr, "	-w wander	change center of locality after wander ticks\n");
	fprintf(stderr, "			(min 100)\n");
	fprintf(stderr, "	-a 		advise system of wandering\n");

	fprintf(stderr, "	-S nsecs	sleep nsecs after each change of locality\n");
	fprintf(stderr, "	-h		dump histograms after each change of locality\n");
	fprintf(stderr, "	-m		use a mapped file\n");
}
char	*optstr = "dhas:S:c:l:n:r:w:b:m";
extern	char *optarg;
int	Debug;
int	id;
int	nprocd = 0;
int	rfactor;
int	burpn, wandern, wanderadvice;
int	sleepn;
int	usemapped;
char	*tfile;
char	Cmd[20];

main(argc, argv)
int	argc;
char	**argv;
{
	int	nreps;
	int	nprocs;
	long	i;
	int	c;

	pagesize = getpagesize();
	strncpy(Cmd, argv[0], sizeof(Cmd) - 1);
	/* defaults */
	scale = 3.0;
	rfactor = 1;
	nprocs = 1;
	nreps =   50000;
	wandern = 50000000;
	burpn  =   1000;

	while ((c = getopt(argc, argv, optstr)) != EOF)
		switch(c) {
		case 'd':
			Debug++;
			break;
		case 'h':
			histograms++;
			break;
		case 'a':
			wanderadvice++;
			break;
		case 's':
			size = atol(optarg);
			break;
		case 'S':
			sleepn = atoi(optarg);
			break;
		case 'c':
			nreps = atoi(optarg);
			break;
		case 'n':
			nprocs = atoi(optarg);
			break;
		case 'l':
			scale = atof(optarg);
			break;
		case 'r':
			rfactor = atoi(optarg);
			break;
		case 'b':
			burpn = atoi(optarg);
			break;
		case 'w':
			wandern = atoi(optarg);
			if (wandern < 5) {
				usage(argv[0]);
				exit(-1);
			}
			break;
		case 'm':
			usemapped++;
			break;
		default:
			fprintf(stderr, "%s: illegal option %c\n", Cmd, c);
			usage(argv[0]);
			exit(-1);
		}


	signal(SIGHUP, sigc);
	signal(SIGINT, sigc);
	if (size == 0) {
		fprintf(stderr, "%s: Must specify size in bytes\n", Cmd);
		usage(argv[0]);
		exit(-1);
	}

	/* if using a mapped file - first create file */
	if (usemapped) {
		int fd;

		if ((tfile = tempnam(NULL, "scrand")) == NULL) {
			fprintf(stderr, "%s: cannot find temp file\n", Cmd);
			exit(1);
		}
		if ((fd = open(tfile, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0) {
			fprintf(stderr, "%s: cannot open temp file", Cmd);
			perror(tfile);
			exit(1);
		}
		if (lseek(fd, size, 0) < 0) {
			fprintf(stderr, "%s: cannot seek to %ld\n", Cmd, size);
			unlink(tfile);
			exit(1);
		}
		if (write(fd, &size, sizeof(size)) != sizeof(size)) {
			fprintf(stderr, "%s: cannot write at end", Cmd);
			perror(" ");
			unlink(tfile);
			exit(1);
		}
#define NEVER 1
#ifdef NEVER
		/* kluge for now.. since mmap doesn't seem to sync NFS 
		 * file correctly 
		 */
		if ((fd = close(fd)) < 0) {
			fprintf(stderr, "%s: cannot close temp file", Cmd);
			perror(tfile);
			exit(1);
		}
		if ((fd = open(tfile, O_RDWR)) < 0) {
			fprintf(stderr, "%s: cannot re-open temp file", Cmd);
			perror(tfile);
			exit(1);
		}
#endif
		if ((base = (int *) mmap(0, size, PROT_READ|PROT_WRITE,
				MAP_SHARED, fd, 0)) == (int *)-1L) {
			fprintf(stderr, "%s: cannot mmap", Cmd);
			perror(" ");
			unlink(tfile);
			exit(1);
		}
	} else {
		if ((base = malloc(size)) == (int *) 0) {
			fprintf(stderr, "%s: cannot allocate %ld bytes\n", argv[0], size);
			exit(1);
		}
	}
	size /= sizeof(struct nums);
	tbase = (struct nums *) base;

	/*
	 * align tbase on page boundary if it isn't ...
	 */
	if ((long)tbase & (sizeof(struct nums) -1L)) {
		size--;
		tbase = (struct nums *)((long)tbase & ~(sizeof(struct nums) -1L));
		tbase++;
	}

	/*
	 * force all pages to be faulted in
	 */
	for (i = 0; i < size; i++)
		tbase[i].hist = 0;

	signal(SIGTERM, dmphist);

	center = size / 2;

	if (Debug) {
		printf("<-- size: %ld -->\n", size);
		fflush(stdout);
	}

	id = getpid();

	if (nprocs <= 0) {
		fprintf(stderr, "nprocs (%d) must be > 0\n");
		exit(2);
	}
	i = nprocs;

	while (--i) {
		if (sproc(slave, PR_SALL, nreps) >= 0) {
			nprocd++;
		}
	}
	slave(nreps);
	while (wait(0) >= 0 || errno == EINTR)
		;
	unlink(tfile);
	exit(0);
}


void
sigc()
{
	unlink(tfile);
	exit(-1);
}

void
slave(nreps)
{
	register int	i;
	register int	burp = 0;
	register int	wander = 0;
	register int	wandert = 0;
	int		rcount;
	long		tp;
	norm_t		rv;
	unsigned long	cycles[2];
	long		otime, ntime;
	int		buffer[10];
	int		myid;

	myid = getpid();
	cycles[0] = 0;
	cycles[1] = 0;
	otime = times(buffer);
	rcount = 1;

	for (i = 0; i < nreps; i++ ) {
		/*
		 * Get a "random" center of locality.
		 */
		locality = rand();
		if (Debug)
			printf("locality %ld -->	", locality);
		while (locality < size)
			locality *= 2;
		while (locality >= size)
			locality -= size;
		if (Debug)
			printf("%ld\n", locality);

		for ( ; i < nreps ; i++ ) {
			norm(&rv);
			tp = ((long)((rv.x1 * center)/scale)) + locality;
			if (tp >= 0 && tp < size) {
				if (tbase[tp].hist == 0) {
					if (rcount >= rfactor)
						tbase[tp].chkval =
						   (long) &tbase[tp];
				}
				else if (tbase[tp].chkval != (long) &tbase[tp]) {
					printf("Verify error at 0x%x\n",
						&tbase[tp]);
				}
				if (rcount >= rfactor)
					tbase[tp].hist++;
			}
			tp = ((long)((rv.x2 * center)/scale)) + locality;
			if (tp >= 0 && tp < size) {
				if (tbase[tp].hist == 0) {
					if (rcount >= rfactor)
						tbase[tp].chkval =
						    (long) &tbase[tp];
				}
				else if (tbase[tp].chkval != (long) &tbase[tp]) {
					printf("Verify error at 0x%x\n",
						&tbase[tp]);
				}
				if (rcount >= rfactor)
					tbase[tp].hist++;
			}

			if (rcount >= rfactor)
				rcount = 1;
			else
				rcount++;

			cycles[0]++;
			if (cycles[0] == 0)
				cycles[1]++;
			if (burp++ >= burpn) {
				printf("%s:%d %u%u cycles completed\n",
					Cmd, myid, cycles[1], cycles[0]);
				fflush(stdout);
				burp = 0;
			}

			if (wandert == 0) {
				ntime = times(buffer);
				wander++;
				if ((ntime - otime) > wandern / 8)
					wandert = wander;
			} else
			if (++wander >= wandert) {
				wander = 0;
				ntime = times(buffer);
				if ((ntime - otime) >= wandern) {
					otime = ntime;
					if (histograms)
						dumphist(i);
					if (wanderadvice) {
						char *addr1, *addr2;
						int  len;
						norm(&rv);
						tp = ((int)((rv.x1 * center) /
							scale)) + locality;
						if (tp >= 0 && tp < size)
							addr1 = (char *)
								&tbase[tp];
						else
							addr1 = (char *)tbase;
						tp = ((int)((rv.x2 * center) /
							scale)) + locality;
						if (tp >= 0 && tp < size)
							addr2 = (char *)
								&tbase[tp];
						else
							addr2 = (char *)
								&tbase[size-1];
						if (addr2 < addr1) {
							len = addr1 - addr2;
							addr1 = addr2;
						} else
							len = addr2 - addr1;
						if (Debug) {
							printf("madvise: %d, %d\n", addr1, len);
							fflush(stdout);
						}
						madvise(addr1, len,
							MADV_DONTNEED);
					}
					if (sleepn)
						sleep(sleepn);
					break;
				}
			}
		}
	}

	if (get_pid() == id) {
		while (nprocd--)
			blockproc(id);
	} else {
		unblockproc(id);
	}
}


void
dmphist()
{
	signal(SIGTERM, dmphist);
	dumphist(0);
}

void
dumphist(i)
{
   register FILE	*fd;
   register long	j;
   char			fbuf[BUFSIZ];
   long			pgno;
   int			hits;

	if (i == 0)
		sprintf(fbuf, "./h%d.TERM", getpid());
	else
		sprintf(fbuf, "./h%d.%d", getpid(), i/100);
	if ((fd = fopen(fbuf, "w")) == NULL) {
		fprintf(stderr, "Can't dump to %s\n", fbuf);
		return;
	}
	fprintf(fd, "steps = %ld, center of locality = %ld, scale = %f\n",
		size, locality, scale);
	pgno = (long)tbase & ~(NBPP-1L);
	hits = 0;
	for (j = 0; j < size; j++) {
		if (((long)&tbase[j] & ~(NBPP-1L)) != pgno) {
			fprintf(fd, "%d\n", hits);
			pgno += NBPP;
			hits = 0;
		}
		if (tbase[j].hist) {
			hits += tbase[j].hist;
			tbase[j].hist = 0;
		}
	}
	/*
	 * Print last (partial) page.
	 * If the num struct just past the last one doesn't
	 * start on a page boundary, we have a partial page left.
	 */
	if ((long)&tbase[j] & (NBPP-1L))
		fprintf(fd, "%d\n", hits);
	fclose(fd);
	return;
}
