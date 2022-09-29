/*
 * istress - stress inode/file handling
 *
 * Test 1 - multiple processes open a file for append and make sure
 *	    that what they write is valid
 * Test 2 - open multiple files that are on the same hash list
 *	    via multiple processes
 */

#include "sys/types.h"
#include "sys/stat.h"
#include "sys/signal.h"
#include "stdio.h"
#include "fcntl.h"
#include "errno.h"
#include "ftw.h"
#include "ctype.h"
#include "limits.h"

extern int errno;
extern char *malloc();

int Opts;
#define SOPT	0x0001			/* size in bytes */
#define NOPT	0x0002			/* # procs */
#define ROPT	0x0004			/* # reps */
#define FOPT	0x0008			/* # files to match */
#define VOPT	0x0010			/* Verbose */
#define ISOPT(x)	((x) & Opts)

int size = 64;				/* size of record in bytes */
int nprocs = 2;				/* # procs to start up */
int nreps = 10;				/* # reps */
int nfile = 2;				/* # files with same hash */
int Verbose = 0;
char tfilename[PATH_MAX];

/* yuck - understand kernel inode hashing */
#define	NHINO	128
#define ihash(X)	((int) (X) & (NHINO-1))

extern int itest1(), itest2();

struct test {
	int (*tfunc)();			/* ptr to function for test */
	char *tdesc;			/* test description */
} Tests[] = {
	itest1, "itest1",
	itest2, "itest2",
};
int maxtests = sizeof(Tests)/sizeof(struct test);


main(argc, argv)
 int argc;
 char **argv;
{
	extern char *optarg;
	extern int optind;
	int tnum;
	int c;

	if (argc <= 1)
		Usage();
		/* NOTREACHED */
	++argv; --argc;
	if (!isdigit(argv[0][0]))
		Usage();
		/* NOTREACHED */

	tnum = atoi(*argv);
	if (tnum > maxtests) {
		fprintf(stderr, "bad test #%d\n", tnum);
		Usage();
		/* NOTREACHED */
	}

	while ((c = getopt(argc, argv, "Vvf:s:n:r:")) != EOF) {
		switch (c) {
		case 's':	/* size of file */
			Opts |= SOPT;
			size = atoi(optarg);
			break;
		case 'n':	/* # processes */
			Opts |= NOPT;
			nprocs = atoi(optarg);
			break;
		case 'f':	/* # files */
			Opts |= ROPT;
			nfile = atoi(optarg);
			break;
		case 'V':	/* verbose */
		case 'v':	/* verbose */
			Opts |= VOPT;
			Verbose++;
			break;
		case 'r':	/* # reps */
			Opts |= ROPT;
			nreps = atoi(optarg);
			break;
		case '?':
		default:
			Usage();
			/* NOTREACHED */
		}
	}

	/* doit */
	Tests[tnum-1].tfunc();

	exit(0);
}

Usage()
{
	fprintf(stderr, "Usage:istress tst# [-n #procs] [-s recsize] [-r reps]\n");
	fprintf(stderr, "\t1 [-n #procs][-s recsz][-r reps] - test APPEND mode\n");
	fprintf(stderr, "\t2 [-n #procs][-f #file][-r reps] - test iget w/ same hash\n");
	exit(-1);
}

/* 
 * synchronization for children : parent sends SIGUSR1 when it has forked
 * all children. Children do a pause till receipt of SIGUSR1.
 */
int chwait = 1;

/* SIGUSR1 signal handler */
void
gochild()
{
	chwait = 0;
}

itest1()
{
	auto int statloc;
	register int fd, pid, i;

	fprintf(stderr, "\nItest1: size:%d nprocs:%d nreps:%d\n",
			size, nprocs, nreps);

	(void)sprintf(tfilename,"/usr/tmp/istress.%d",getpid());
	/* get file ready */
	if ((fd = open(tfilename, O_TRUNC|O_CREAT|O_RDWR, 0666)) < 0) {
		(void)sprintf(tfilename,"/tmp/istress.%d",getpid());
		if ((fd = open(tfilename, O_TRUNC|O_CREAT|O_RDWR, 0666)) < 0) {
			perror("itest1:open");
			return(-1);
		}
	}
	close(fd);
	/*
	 * Set the handler now so that the children inherit it.
	 */
	signal(SIGUSR1, gochild);
	setpgrp();

	/* fork off n processes */
	for (i = 0; i < nprocs; i++) {
		if ((pid = fork()) < 0) {
			perror("itest1:fork");
			kill(0, 15);
			return(-1);
		} else if (pid == 0) {
			/* child */
			sighold(SIGUSR1);
			if (chwait)
				sigpause(SIGUSR1);
			sigrelse(SIGUSR1);
			exit(doitest1(i, size, nreps));
		}
	}
	/* start them all going */
	kill(0, SIGUSR1);

	/* wait for everybody */
	for (;;)
		if (wait(&statloc) < 0 && errno == ECHILD)
			break;
	return(0);
}

/*
 * doitest1 - write via APPEND and check results
 * for more than 1 rep - check ALL writes (from beginning of file)
 */
doitest1(id, sz, nreps)
 int id;		/* makes me different */
 int sz;		/* size of record to write */
 int nreps;		/* # times to do it */
{
	extern char *malloc();
	char *buf, *vbuf;
	int *offsets;
	int fd;
	register int i, j;
	register int off;

	if (Verbose)
		fprintf(stderr, "doit1:id:%d sz:%d rep:%d\n", id, sz, nreps);

	/* get buffers */
	if ((buf = malloc(sz)) == NULL) {
		fprintf(stderr, "%d: bad malloc for buf\n", id);
		return(-1);
	}
	if ((vbuf = malloc(sz)) == NULL) {
		free(buf);
		fprintf(stderr, "%d: bad malloc for vbuf\n", id);
		return(-1);
	}
	/* get space to store offsets */
	if ((offsets = (int *) malloc(nreps * sizeof(int))) == NULL) {
		free(buf);
		free(vbuf);
		fprintf(stderr, "%d: bad malloc for offsets\n", id);
		return(-1);
	}

	/* fill buffer unique to me */
	for (i = 0; i < sz; i++)
		buf[i] = id + '0';
	buf[sz - 1] = '\n';

	/* open file for APPEND */
	if ((fd = open(tfilename, O_RDWR|O_APPEND)) < 0) {
		fprintf(stderr, "%d: bad open\n", id);
		goto cleanup;
	}

	for (j = 0; j < nreps; j++) {
		/* write out buffer */
		if (write(fd, buf, sz) != sz) {
			fprintf(stderr, "%d: bad write\n", id);
			goto cleanup;
		}

		/* get current offset and subtract to find start */
		offsets[j] = lseek(fd, 0, 1) - sz;
		if (Verbose)
			fprintf(stderr, "id:%d j:%d offset:%d\n", id, j, offsets[j]);

		/* verify contents */
		for (i = 0; i <= j; i++) {
			/* seek to first offset */
			if (lseek(fd, offsets[i], 0) < 0) {
				fprintf(stderr, "%d: bad lseek\n", id);
				goto cleanup;
			}

			if (read(fd, vbuf, sz) != sz) {
				fprintf(stderr, "%d: bad reread\n", id);
				goto cleanup;
			}

			if (strncmp(buf, vbuf, sz) != 0) {
				fprintf(stderr, "%d: bad compare wanted:%c got:%c\n",
					id, buf[0], vbuf[0]);
				goto cleanup;
			}
		}
	}

	free(buf);
	free(vbuf);
	free(offsets);
	close(fd);
	return(0);

cleanup:
	fflush(stderr);
	free(buf);
	free(vbuf);
	free(offsets);
	close(fd);
	return(-1);
}

/* junk for ftw */
struct fino {
	char *name;
	long ino;
};
struct fino *gfnames;	/* names and inodes of found files */
int gnf;		/* # found files w/ same ihash */
int gnmax;		/* # files looking for */
struct stat seed;	/* file to match on */

/*
 * itest 2 - stress opening/closing n files with same hash characteristics
 * WARNING - assumes knowledge of kernel inode hash algorithm
 */
itest2()
{
	auto int statloc;
	register int pid, i;
	struct fino *fnames;

	fprintf(stderr, "\nItest2: nfile:%d nprocs:%d nreps:%d\n",
			nfile, nprocs, nreps);

	/* get space for file names */
	if ((fnames = (struct fino *) malloc(nfile*sizeof(struct fino))) == NULL) {
		fprintf(stderr, "bad malloc for fnames\n");
		return(-1);
	}
	/* find files with same hash characteristics */
	if (findsamehash(fnames, nfile) != 0 || gnf != nfile) {
		/* shucks - couldn't find 'em */
		fprintf(stderr, "couldn't find %d same hash files\n", nfile);
		free(fnames);
		return(-1);
	}
	/*
	 * Set the handler now so that the children inherit it.
	 */
	signal(SIGUSR1, gochild);
	setpgrp();

	/* fork off n processes */
	for (i = 0; i < nprocs; i++) {
		if ((pid = fork()) < 0) {
			perror("itest1:fork");
			kill(0, 15);
			return(-1);
		} else if (pid == 0) {
			/* child */
			sighold(SIGUSR1);
			if (chwait)
				sigpause(SIGUSR1);
			sigrelse(SIGUSR1);
			exit(doitest2(i, nfile, fnames, nreps));
		}
	}
	/* start them all going */
	kill(0, SIGUSR1);

	/* wait for everybody */
	for (;;)
		if (wait(&statloc) < 0 && errno == ECHILD)
			break;
	return(0);
}

doitest2(id, nfile, fnames, nreps)
 int id;		/* who are we */
 int nfile;		/* how many files to do */
 struct fino *fnames;	/* names of files to do */
 int nreps;		/* how many times */
{
	register int i, j;
	register int fd;

	if (Verbose)
		fprintf(stderr, "doit2:id:%d nf:%d rep:%d\n", id, nfile, nreps);

	for (j = 0; j < nreps; j++) {
		for (i = 0; i < nfile; i++) {
			if ((fd = open(fnames[i].name, O_RDONLY|O_NDELAY)) < 0) {
				fprintf(stderr, "%d:cant open %s\n",
					id, fnames[i].name);
				return(-1);
			}
			close(fd);
		}
	}
	return(0);
}

/*
 * findsamehash - look for nfile files whose inodes will hash to the
 * same hash chain
 */
findsamehash(fnames, nf)
 struct fino *fnames;	/* where to store names */
 int nf;		/* how many to search for */
{
	extern int ftwfn();
	static char *seeds[] = {
		"/etc/init",
		"/bin/sh",
		"/bin/cp",
		0
	};
	static char *seeds1[] = {
		"/usr/etc/netstat",
		"/usr/bin/uucp",
		0
	};
	register char **try;

	gfnames = fnames;
	gnmax = nf;

	/* search through seed files looking for possible file with enough
	 * matches
	 */
	gnf = 0;	/* start at 1 since start with seed */
	for (try = seeds; *try; try++) {
		if (stat(*try, &seed) < 0) {
			fprintf(stderr, "cannot stat /etc/init!\n");
			return(-1);
		}
		fnames[gnf].name = *try;
		fnames[gnf].ino = seed.st_ino;
		fprintf(stderr, "looking for matches to ino:%d (0x%x)\n",
			seed.st_ino, seed.st_ino);

		/* start looking in /etc */
		if (ftw("/etc", ftwfn, 10) < 0)
			return(-1);
		if (gnf == gnmax)
			return(0);
		if (ftw("/bin", ftwfn, 10) < 0)
			return(-1);
		if (gnf == gnmax)
			return(0);
		if (ftw("/lib", ftwfn, 10) < 0)
			return(-1);
		if (gnf == gnmax)
			return(0);

	}
	for (try = seeds1; *try; try++) {
		if (stat(*try, &seed) < 0) {
			fprintf(stderr, "cannot stat /usr/etc/netstat\n");
			return(-1);
		}
		fnames[gnf].name = *try;
		fnames[gnf].ino = seed.st_ino;
		fprintf(stderr, "looking for matches to ino:%d (0x%x)\n",
			seed.st_ino, seed.st_ino);

		if (ftw("/usr", ftwfn, 10) < 0)
			return(-1);
		if (gnf == gnmax)
			return(0);  
	}
	return(0);
}

ftwfn(fname, st, code)
 char *fname;		/* name of file */
 struct stat *st;
 int code;
{
	register int i;

	if (code == FTW_NS || code == FTW_DNR)
		return(0);
	if (st->st_dev != seed.st_dev)
		return(0);
	if (st->st_ino == seed.st_ino)
		return(0);
	if (ihash(st->st_ino) != ihash(seed.st_ino))
		return(0);
	
	/* so far so good - see if already have it (e.g. a link) */
	for (i = 0; i < gnf; i++)
		if (st->st_ino == gfnames[i].ino)
			return(0);

	/* found one! */
	fprintf(stderr, "file:%s ino:%d (0x%x) matches!\n",
		fname, st->st_ino, st->st_ino);

	gnf++;
	if ((gfnames[gnf].name = malloc(strlen(fname)+1)) == NULL) {
		fprintf(stderr, "bad malloc on fname\n");
		return(-1);
	}
	strcpy(gfnames[gnf].name, fname);
	gfnames[gnf].ino = st->st_ino;
	if (gnf == gnmax)
		return(gnf);
	return(0);
}
