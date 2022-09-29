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

#ident	"$Revision: 1.4 $ $Author: yohn $"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>
#include <ulocks.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include "stress.h"

usptr_t	*handle;

#define FSZ	0x20000
#define FSS	0x02000

char *Cmd;
char	*semfile;
char	*agfile;
usema_t	*truncsema;
pid_t	checkerid, truncid;
int	agfd, agrwfd;
int	nreps = 25;
caddr_t	vaddr;
static char c = 'a';
pid_t ppid;
int keepgoing;
int nblockproc;
int verbose;
int off;
int pgsize;

void catchit(int, int, struct sigcontext *);
void storer(int, int);
void checker(void *), trunc(void *);
void loader(off_t start, off_t len, int id);
void reader(off_t start, off_t len, int id);

int
main(int argc, char **argv)
{
	void (*routine)(off_t, off_t, int);
	int c, i, errflg = 0;

	Cmd = errinit(argv[0]);
	ppid = getpid();

	while ((c = getopt(argc, argv, "vn:")) != -1)
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'n':
			nreps = atoi(optarg);
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, "Usage: aglock [-v][-n loops]\n");
		exit(1);
	}

	semfile = tempnam(NULL, "aglock");
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	handle = usinit(semfile);
	if (handle == (usptr_t *)0)
		errprintf(ERR_ERRNO_EXIT, "usinit of %s failed", semfile);

	truncsema = usnewsema(handle, 1);
	if (truncsema == (usema_t *)0)
		errprintf(ERR_ERRNO_EXIT, "usnewsema failed");

	agfile = tempnam(NULL, "aglock");
	agfd = open(agfile, O_RDWR | O_CREAT, 0777);
	if (agfd < 0)
		errprintf(ERR_ERRNO_EXIT, "create %s", agfile);

	agrwfd = open(agfile, O_RDWR);
	if (agrwfd < 0) {
		errprintf(ERR_ERRNO_RET, "open %s for write failed", agfile);
		goto bad;
	}

	srand(time((time_t *)0));
	pgsize = getpagesize();

	keepgoing = 1;

	printf("%s: doing %d reps\n", argv[0], nreps);
	if (argc <= 2) {
		off = (rand() % FSS) & ~(pgsize -1);

		vaddr = mmap(0, FSZ, PROT_READ|PROT_WRITE,
			     MAP_SHARED|MAP_AUTOGROW, agrwfd, off);
		if (vaddr == (caddr_t) -1L) {
			errprintf(ERR_ERRNO_RET, "mmap tfile");
			goto bad;
		} else if (verbose)
			printf("aglock:mapped at 0x%x from 0x%x\n", vaddr, off);
		routine = loader;
	} else
		routine = reader;

	checkerid = sproc(checker, PR_SALL, 0);
	if (checkerid < 0) {
		perror("aglock:can't sproc");
		unlink(agfile);
		exit(1);
	}
	nblockproc++;

	truncid = sproc(trunc, PR_SALL, 0);
	if (truncid < 0) {
		perror("aglock:can't sproc");
		kill(checkerid, SIGKILL);
		unlink(agfile);
		exit(1);
	}
	nblockproc++;

	for (i = 0; i < nreps; i++) {
		off_t	start = rand() % FSZ;
		off_t	len = rand() % (FSZ - start);

		(*routine)(start, len, i);
		sginap(20);
	}

	keepgoing = 0;
	while (nblockproc--) {
		blockproc(ppid);
	}
	usfreesema(truncsema, handle);
	unlink(agfile);
	exit(0);

bad:
	unlink(agfile);
	abort();
}

char	*gs;
off_t	glen;
char	bytebucket;

void
loader(off_t start, off_t len, int id)
{
	volatile char *bb;
	off_t pinstart, pinlen;

	bb = &bytebucket;

	if (verbose)
		printf("aglock:loading(%d) 0x%x bytes from %x for %x...",
					id, start + off, len);
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

	pinstart = rand() % FSZ;
	if (pinstart >= start + len)
		pinstart = start + len / 2;
	pinlen = rand() % (FSZ - pinstart);

	while (mpin(vaddr + pinstart, pinlen)) {
		pinlen -= pgsize;
		if (pinlen <= 0)
			break;
	}
	if (verbose)
		printf("aglock:pinned @%x for %x...", pinstart, pinlen);
	fflush(stdout);
}

void
storer(int sig, int code)
{
	int	j;
	char	*s = gs;

	/* re-enable signal handling here to catch if storing fails */
	signal(SIGSEGV, catchit);

	if (code == EBUSY) {
		if (verbose)
			printf("unlocking pages (storer)...");
		fflush(stdout);
		munpin(vaddr, FSZ);
		signal(SIGSEGV, storer);
		return;
	}

	if (verbose)
		printf("aglock:storing 0x%x bytes of %c...(got error %d) ",
				glen, c, code);
	fflush(stdout);

	for (j = 0; j < glen; j++)
		*s++ = c;

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

void
reader(off_t start, off_t len, int id)
{
	int	bytesread;
	char	buf[FSZ];

	printf("aglock:reading from %x for %x...", start, len);
	fflush(stdout);
	if (lseek(agrwfd, start, 0) < 0) {
		perror("aglock:lseek");
		exit(1);
	}
	bytesread = read(agrwfd, buf, len);
	if (bytesread < len) {
		register int	j;

		len = len - bytesread;
		printf("aglock:writing 0x%x bytes of %c...", len, c);
		fflush(stdout);
		for (j = 0; j < len; j++)
			buf[j] = c;

		if (write(agrwfd, buf, len) != len) {
			perror("aglock:reader write");
			exit(1);
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
catchit(int sig, int code, struct sigcontext *scp)
{
	if (code == EBUSY) {
		signal(SIGSEGV, catchit);
		if (verbose)
			printf("unlocking pages (catchit)...");
		fflush(stdout);
		munpin(vaddr, FSZ);
		return;
	}

	errprintf(ERR_RET, "caught unexpected signal %d - code %d:%s",
		sig, code, strerror(code));
	keepgoing = 0;
	blockproc(ppid);
	blockproc(ppid);
	usfreesema(truncsema, handle);
	abort();
}

/*
 * one sproc slave does random truncates
 */
void
trunc(void *arg)
{
	do {
		int	tval;

		tval = rand() % FSZ;
		if (verbose)
			printf("aglock:truncating to 0x%x...", tval);
		fflush(stdout);
		uspsema(truncsema);
		if (ftruncate(agfd, tval)) {
			usvsema(truncsema);
			errprintf(ERR_ERRNO_RET, "ftruncate");
			unblockproc(ppid);
			abort();
		}
		usvsema(truncsema);
		sginap(40);
	} while (keepgoing);

	unblockproc(ppid);
}

/*
 * one sproc guy does checks
 */
void
checker(void *arg)
{
	struct stat statb;
	register int fd, size, j;
	char	buf[FSZ];

	fd = open(agfile, O_RDONLY);
	if (fd < 0) {
		errprintf(ERR_ERRNO_RET, "open");
		unblockproc(ppid);
		abort();
	}
	do {
		sleep(1);
		uspsema(truncsema);
		if (stat(agfile, &statb) < 0) {
			usvsema(truncsema);
			errprintf(ERR_ERRNO_RET, "stat");
			unblockproc(ppid);
			abort();
		}
		if (verbose)
			printf("aglock:checking 0x%x...", statb.st_size);
		fflush(stdout);
		lseek(fd, 0, 0);
		size = read(fd, buf, statb.st_size);
		if (size != statb.st_size) {
			errprintf(ERR_RET, "got 0x%x bytes, not 0x%x",
				size, statb.st_size);
			abort();
		}
		usvsema(truncsema);
		for (j = 0; j < size; j++) {
			if (buf[j] != 0 && (buf[j] < 'a' || buf[j] > 'z')) {
				errprintf(ERR_RET,
					"checker: got %d: byte 0x%x of 0x%x!",
					buf[j], j, size);
				unblockproc(ppid);
				abort();
			}
		}
	} while (keepgoing);

	unblockproc(ppid);
}
