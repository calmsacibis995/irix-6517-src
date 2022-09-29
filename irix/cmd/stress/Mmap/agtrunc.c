/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.11 $ $Author: jwag $"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <ulocks.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include "stress.h"

usptr_t	*handle;

#define FSZ	0x80000
#define FSS	0x20000

void checker(void *a);
void trunc(void *a);
void catchit(int sig, int code);
void reader(int start, int len, int);
void storer(int sig, int code);
void loader(int start, int len, int id);
void bagit(void);

char	*semfile;
char	*agfile;
usema_t	*truncsema;
int	checkerid, truncid;
int	agfd, agrwfd;
int	nreps = 25;
caddr_t	vaddr;
static char c = 'a';
int ppid;
int keepgoing;
int nblockproc;
int off;
int verbose;
int doreader;
char *Cmd;

int
main(int argc, char **argv)
{
	void  (*routine)(int, int, int);
	int errflg = 0, i, ch;

	Cmd = errinit(argv[0]);
	while ((ch = getopt(argc, argv, "vrl:")) != -1)
		switch (ch) {
		case 'r':
			doreader++;
			break;
		case 'v':
			verbose++;
			break;
		case 'l':
			nreps = atoi(optarg);
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, "Usage: agtrunc [-v][-l loops]\n");
		exit(1);
	}

	ppid = getpid();
	semfile = tempnam(NULL, "agtrunc");
	handle = usinit(semfile);
	if (handle == (usptr_t *)0) {
		errprintf(ERR_ERRNO_EXIT, "usinit");
		/* NOTREACHED */
	}
	truncsema = usnewsema(handle, 1);
	if (truncsema == (usema_t *)0) {
		errprintf(ERR_ERRNO_EXIT, "usnewsema");
		/* NOTREACHED */
	}

	agfile = tempnam(NULL, "Tmagt");
	agfd = open(agfile, O_RDWR | O_CREAT, 0777);
	if (agfd < 0) {
		errprintf(ERR_ERRNO_EXIT, "create file");
		/* NOTREACHED */
	}
	agrwfd = open(agfile, O_RDWR);
	if (agrwfd < 0) {
		perror("agtrunc:create tfile");
		bagit();
		/* NOTREACHED */
	}

	srand(time(0));

	keepgoing = 1;

	printf("%s: doing %d reps\n", Cmd, nreps);
	if (!doreader) {
		off = (rand() % FSS) & ~(getpagesize() -1);

		vaddr = mmap(0, FSZ, PROT_READ|PROT_WRITE,
			     MAP_SHARED|MAP_AUTOGROW, agrwfd, off);
		printf("agtrunc:mapped at 0x%x from 0x%x\n", vaddr, off);
		if (vaddr == (caddr_t) -1L) {
			perror("agtrunc:mmap tfile");
			bagit();
			/* NOTREACHED */
		}
		routine = loader;
	} else
		routine = reader;

	checkerid = sproc(checker, PR_SALL, 0);
	if (checkerid < 0) {
		perror("agtrunc:sproc");
		bagit();
		/* NOTREACHED */
	}
	nblockproc++;

	truncid = sproc(trunc, PR_SALL, 0);
	if (truncid < 0) {
		perror("agtrunc:sproc");
		kill(checkerid, SIGKILL);
		bagit();
		/* NOTREACHED */
	}
	nblockproc++;

	for (i = 0 ; i < nreps ; i++ ) {
		int	start = rand() % FSZ;
		int	len = rand() % (FSZ - start);

		(*routine)(start, len, i);
		sginap(20);
	}

	keepgoing = 0;
	while (nblockproc--) {
		blockproc(ppid);
	}
	usfreesema(truncsema, handle);
	unlink(agfile);
	unlink(semfile);
	printf("%s: PASSED\n", Cmd);
	return 0;
}

void
bagit(void)
{
	unlink(agfile);
	unlink(semfile);
	abort();
	/* NOTREACHED */
}

char	*gs;
int	glen;
char	bytebucket;

void
loader(int start, int len, int id)
{
	volatile char	*bb;

	bb = &bytebucket;

	if (verbose)
		printf("agtrunc:loading(%d) 0x%x bytes from 0x%x...", id, len, start+off);
	fflush(stdout);

	gs = vaddr + start;

	if (signal(SIGSEGV, storer) == SIG_ERR) {
		keepgoing = 0;
		blockproc(ppid);
		blockproc(ppid);
		usfreesema(truncsema, handle);
		exit(1);
	}
	fflush(stdout);

	for (glen = len; glen > 0; glen--) {
		*bb = *gs;
		gs++;
	}
}

/* ARGSUSED */
void
storer(int sig, int code)
{
	int	j;
	char	*s = gs;

	/* re-enable signal handling here to catch if storing fails */
	signal(SIGSEGV, catchit);
	if (verbose)
		printf("agtrunc:storing 0x%x bytes of %c...(got error %d)", glen, c, code);
	fflush(stdout);

	for (j = 0; j < glen; j++) {
		*s++ = c;
	}
	msync(gs, glen, 0);

	if (c == 'z')
		c = 'a';
	else
		c++;

	if (signal(SIGSEGV, storer) == SIG_ERR) {
		keepgoing = 0;
		blockproc(ppid);
		blockproc(ppid);
		usfreesema(truncsema, handle);
		exit(1);
	}
}

/* ARGSUSED */
void
reader(int start, int len, int id)
{
	ssize_t	bytesread;
	char	buf[FSZ];

	if (verbose) {
		printf("agtrunc:reading 0x%x bytes from 0x%x...", len, start);
		fflush(stdout);
	}
	if (lseek(agrwfd, start, 0) < 0) {
		errprintf(ERR_ERRNO_EXIT, "reader lseek");
		/* NOTREACHED */
	}
	bytesread = read(agrwfd, buf, len);
	if (bytesread < len) {
		register int	j;

		len = len - (int)bytesread;
		if (verbose) {
			printf("agtrunc:writing 0x%x bytes of %c...", len, c);
			fflush(stdout);
		}
		for (j = 0; j < len; j++) {
			buf[j] = c;
		}

		if (write(agrwfd, buf, len) != len) {
			errprintf(ERR_ERRNO_EXIT, "reader write");
			/* NOTREACHED */
		}

		if (c == 'z')
			c = 'a';
		else
			c++;
	}
}

/*
 * signal handler when parent should NOT get a signal - implies
 * file system full or something
 */
void
catchit(int sig, int code)
{
	setoserror(code);
	errprintf(ERR_ERRNO_RET, "caught unexpected signal %d - code", sig);
	keepgoing = 0;
	blockproc(ppid);
	blockproc(ppid);
	usfreesema(truncsema, handle);
	abort();
}

/*
 * one sproc slave does random truncates
 */
/* ARGSUSED */
void
trunc(void *a)
{
	do {
		int	tval;

		tval = rand() % FSZ;
		if (verbose) {
			printf("agtrunc:truncating to 0x%x...", tval);
			fflush(stdout);
		}
		uspsema(truncsema);
		if (ftruncate(agfd, tval)) {
			errprintf(ERR_ERRNO_RET, "trunc - ftruncate");
			usvsema(truncsema);
			unblockproc(ppid);
			abort();
			/* NOTREACHED */
		}
		usvsema(truncsema);
		sginap(30);
	} while (keepgoing);

	unblockproc(ppid);
}

/*
 * one sproc guy does checks
 */
/* ARGSUSED */
void
checker(void *a)
{
	struct stat	statb;
	register int	fd, j;
	ssize_t size;
	char	buf[FSZ];

	fd = open(agfile, O_RDONLY);
	if (fd < 0) {
		errprintf(ERR_ERRNO_RET, "checker open");
		unblockproc(ppid);
		abort();
	}
	do {
		sleep(1);
		uspsema(truncsema);
		if (stat(agfile, &statb) < 0) {
			errprintf(ERR_ERRNO_RET, "checker stat");
			usvsema(truncsema);
			unblockproc(ppid);
			abort();
		}
		if (verbose) {
			printf("agtrunc:checking 0x%x...", statb.st_size);
			fflush(stdout);
		}
		lseek(fd, 0, 0);
		size = read(fd, buf, statb.st_size);
		if (size != statb.st_size) {
			fprintf(stderr,
				"agtrunc:(got 0x%x bytes, not 0x%x) ",
				size, statb.st_size);
			fflush(stderr);
		}
		usvsema(truncsema);
		for (j = 0; j < size; j++) {
			if (buf[j] != 0 && (buf[j] < 'a' || buf[j] > 'z')) {
				errprintf(ERR_RET, 
					"checker: got %d: byte 0x%x of 0x%x!\n",
					buf[j], j, size);
				unblockproc(ppid);
				abort();
			}
		}
	} while (keepgoing);

	unblockproc(ppid);
}
