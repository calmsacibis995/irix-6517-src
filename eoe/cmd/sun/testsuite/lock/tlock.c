/*	@(#)tlock.c	1.2 97/01/03 Connectathon Testsuite	*/
/*
 *	System V NFS
 *
 *	Copyright 1986, 1987, 1988, 1989 Lachman Associates, Incorporated (LAI)
 *
 *	All Rights Reserved.
 *
 *	The copyright above and this notice must be preserved in all
 *	copies of this source code.  The copyright above does not
 *	evidence any actual or intended publication of this source
 *	code.
 *
 *	This is unpublished proprietary trade secret source code of
 *	Lachman Associates.  This source code may not be copied,
 *	disclosed, distributed, demonstrated or licensed except as
 *	expressly authorized by Lachman Associates.
 */

/*
 * #ifndef lint
 * static char SysVr3NFSID[] = "@(#)tlock.c	4.11 System V NFS source";
 * #endif
 */

/*
 * Test program for System V, Release 3 record locking using lockf().
 */

/*
 * Large file support, added June 1996
 *
 * These tests exercise the large file locking capabilities defined
 * by the large file summit proposal as defined in the final draft:
 * http://www.sas.com:80/standards/large.file/x_open.20Mar96.html
 *
 * The tests done may be modified by the following flags:
 *	LF_SUMMIT:	accept EOVERFLOW in place of EINVAL where
 *			large file summit spec calls for that, rather
 *			warning about it
 *	LARGE_LOCKS:	use 64-bit API to test locking in [0-2^^63]
 *			range rather than the [0-2^^31] range
 */

#define IRIX

#ifdef LARGE_LOCKS
#define _FILE_OFFSET_BITS	64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#if defined(__STDC__)
#include <stdarg.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/times.h>
#ifdef MMAP
#include <sys/mman.h>
#endif

#if defined(LARGE_LOCKS) && !defined(_LFS64_LARGEFILE)
/* This machine cannot compile the 64 bit version of this test. */
#endif

#ifndef HZ
#define HZ	60		/* a common default */
#endif

/*
 * Implementation dependent constant denoting maximum eof.
 */
#define MAXEOF32	(0x7fffffff)	/* System V.3 */
#define MAXEOF64	(0x7fffffffffffffff)	/* NLM V4 */
#ifdef LARGE_LOCKS
#define MAXEOF	MAXEOF64
#else
#define MAXEOF  MAXEOF32
#endif

#define PARENT	0		/* Who am I? */
#define CHILD	1

#define PASS	0		/* Passed test. */
#define EQUAL	-1		/* Compared equal. */
#define UNEQUAL	-2		/* Compared unequal. */

#define WARN	1		/* Warning, may be a problem. */
#define FATAL	2		/* Fatal testing error. */

#define END	(0)		/* A bit more readable. */

#define DO_TEST(n)	((testnum == 0) || (testnum == (n)))
#define DO_RATE(n)	((ratetest > 0) || (testnum == (n)))
#define DO_MAND(n)	((mandtest > 0) || (testnum == (n)))

#define DO_UNLINK	1
#define JUST_CLOSE	0

int ratetest = 0;
int ratecount = 1000;
int mandtest = 0;

int wait_time = 3;

char arr[1];		/* dummy buffer for pipe */

int parentpipe[2];
int childpipe[2];

char testfile[256];	/* file for locking test */
char *filepath = ".";
int testfd;
#ifdef MMAP
caddr_t mappedaddr;	/* address file is mapped to (some tests) */
off_t mappedlen;
#endif

int testnum;	/* current test number */
int passnum;	/* current pass number */
int passcnt;	/* repeat pass count */
int cumpass;	/* pass for all passes */
int cumwarn;	/* warn for all passes */
int cumfail;	/* fail for all passes */
int tstpass;	/* pass for last pass */
int tstwarn;	/* warn for last pass */
int tstfail;	/* fail for last pass */

int parentpid, childpid;
int who;

#ifdef O_SYNC
#define OPENFLAGS	(O_CREAT | O_RDWR | O_SYNC)
#else
#define OPENFLAGS	(O_CREAT | O_RDWR)
#endif
#define OPENMODES	(0666)
#define MANDMODES	(02666)

void close_testfile();
void comment();

void
initialize()
{

	parentpid = getpid();
	sprintf(&testfile[0], "%s/lockfile%d", filepath, parentpid);

	printf("Creating parent/child synchronization pipes.\n");
	pipe(parentpipe);
	pipe(childpipe);

	fflush(stdout);

#ifdef MMAP
	mappedlen = sysconf(_SC_PAGESIZE);
#endif
}

void
testreport(nok)
	int nok;
{
	FILE *outf;
	char *sp;

	cumpass += tstpass;
	cumwarn += tstwarn;
	cumfail += tstfail;
	outf = (nok ? stderr : stdout);
	sp = ((who == PARENT) ? "PARENT" : " CHILD");
	fprintf(outf, "\n** %s pass %d results: ", sp, passnum);
	fprintf(outf, "%d/%d pass, %d/%d warn, %d/%d fail (pass/total).\n",
		tstpass, cumpass, tstwarn, cumwarn, tstfail, cumfail);
	tstpass = tstwarn = tstfail = 0;
	fflush(outf);
}

void
testexit(nok)
	int nok;
{

	close_testfile(DO_UNLINK);
	if (nok) {
		testreport(1);
	}
	if (who == PARENT) {
		signal(SIGCLD, SIG_DFL);
		if (nok) {
			signal(SIGINT, SIG_IGN);
			kill(childpid, SIGINT);
		}
		wait((int *) 0);
	} else {
		if (nok) {
			signal(SIGINT, SIG_IGN);
			kill(parentpid, SIGINT);
		}
	}
	exit(nok);
	/* NOTREACHED */
}

/* ARGSUSED */
void
parentsig(sig)
	int sig;
{

	testexit(1);
}

/* ARGSUSED */
void
childsig(sig)
	int sig;
{

	testexit(1);
}

/* ARGSUSED */
void
childdied(sig)
	int sig;
{

	comment("Child died");
}

/* VARARGS2 */
void
header(test, fmt, arg1, arg2, arg3, arg4)
	int test;
	char *fmt;
#if defined(OSF1) || defined(IRIX)
	void *arg1, *arg2, *arg3, *arg4;
#else
	int arg1, arg2, arg3, arg4;
#endif
{

	printf("\nTest #%d - ", test);
	printf(fmt, arg1, arg2, arg3, arg4);
	printf("\n");
	fflush(stdout);
}

/* VARARGS1 */
void
comment(fmt, arg1, arg2, arg3, arg4)
	char *fmt;
#if defined(OSF1) || defined(IRIX)
	void *arg1, *arg2, *arg3, *arg4;
#else
	int arg1, arg2, arg3, arg4;
#endif
{

	printf("\t%s", ((who == PARENT) ? "Parent: " : "Child:  "));
	printf(fmt, arg1, arg2, arg3, arg4);
	printf("\n");
	fflush(stdout);
}

void
childwait()
{

	if (read(parentpipe[0], arr, 1) != 1) {
		perror("tlock: child pipe read");
		testexit(1);
	}
}

void
childfree(wait)
	int wait;
{

	if (write(parentpipe[1], arr, 1) != 1) {
		perror("tlock: childfree pipe write");
		testexit(1);
	}
	if (wait)
		sleep(wait);
}

void
parentwait()
{

	if (read(childpipe[0], arr, 1) != 1) {
		perror("tlock: parentwait pipe read");
		testexit(1);
	}
}

void
parentfree(wait)
	int wait;
{

	if (write(childpipe[1], arr, 1) != 1) {
		perror("tlock: child pipe write");
		testexit(1);
	}
	if (wait)
		sleep(wait);
}

static char tmpstr[16];

char *
terrstr(err)
	int err;
{

	switch (err) {
	case UNEQUAL:
		return("unequal");
	case EQUAL:
		return("equal");
	case PASS:
		return("success");
	case EAGAIN:
		return("EAGAIN");
	case EBADF:
		return("EBADF");
	case EACCES:
		return("EACCES");
	case EFAULT:
		return("EFAULT");
	case EINVAL:
		return("EINVAL");
	case EOVERFLOW:
		return("EOVERFLOW");
	case EFBIG:
		return("EFBIG");
	case EDEADLK:
		return("EDEADLK");
#ifdef ECOMM
	case ECOMM:
		return("ECOMM");
#endif
#ifdef ENOLINK
	case ENOLINK:
		return("ENOLINK");
#endif
	default:
		sprintf(tmpstr, "errno=%d", err);
		return(tmpstr);
	}
}

void
report(num, sec, what, offset, length, pass, result, fail)
	int num;			/* test number */
	int sec;			/* test section */
	char *what;
	off_t offset;
	off_t length;
	int pass;			/* expected result */
	int result;			/* actual result */
	int fail;			/* fail or warning */
{

	printf("\t%s", ((who == PARENT) ? "Parent: " : "Child:  "));
#ifdef LARGE_LOCKS
	printf("%d.%-2d - %s [%16llx,", num, sec, what, offset);
#else
	printf("%d.%-2d - %s [%8lx,", num, sec, what, (void *)offset);
#endif
	if (length) {
#ifdef LARGE_LOCKS
		printf("%16llx] ", (long long)length);
#else
		printf("%8lx] ", (long)length);
#endif
	} else {
#ifdef LARGE_LOCKS
		printf("          ENDING] ");
#else
		printf("  ENDING] ");
#endif
	}
	if (pass == result) {
		printf("PASSED.\n");
		tstpass++;
	} else if (pass == EOVERFLOW && result == EINVAL) {
		printf("WARNING!\n");
		comment("**** Expected %s, returned %s...",
			terrstr(pass), terrstr(result));
		comment("**** Okay if expecting pre-large file semantics.");
		tstwarn++;
	} else if (pass == EINVAL && result == EOVERFLOW) {
		printf("WARNING!\n");
		comment("**** Expected %s, returned %s...",
			terrstr(pass), terrstr(result));
		comment("**** Okay if expecting large file semantics.");
		tstwarn++;
	} else if (fail == WARN) {
		printf("WARNING!\n");
		comment("**** Expected %s, returned %s...",
			terrstr(pass), terrstr(result));
		comment("**** Probably Sun3.2 semantics not SVVS.");
		tstwarn++;
	} else {
		printf("FAILED!\n");
		comment("**** Expected %s, returned %s...",
			terrstr(pass), terrstr(result));
		if (pass == PASS && result == EFBIG)
			comment("**** Filesystem doesn't support large files.");
		else
			comment("**** Probably implementation error.");
		tstfail++;
		testexit(1);
	}
	fflush(stdout);
}

char *
tfunstr(fun)
	int fun;
{

	switch (fun) {
	case F_ULOCK:
		return("F_ULOCK");
	case F_LOCK:
		return("F_LOCK ");
	case F_TLOCK:
		return("F_TLOCK");
	case F_TEST:
		return("F_TEST ");
	default:
		fprintf(stderr, "tlock: unknown lockf() F_<%d>.\n", fun);
		testexit(1);
	}
	/* NOTREACHED */
}

void
open_testfile(flags, modes)
	int flags;
	int modes;
{

	testfd = open(testfile, flags, modes);
	if (testfd < 0) {
		perror("tlock: open");
		testexit(1);
	}
}

void
close_testfile(cleanup)
	int cleanup;
{

	if (cleanup == JUST_CLOSE)
		comment("Closed testfile.");
	close(testfd);
	if (cleanup == DO_UNLINK)
		(void) unlink(testfile);
}

void
write_testfile(datap, offset, count)
	char *datap;
	off_t offset;
	int count;
{

	(void) lseek(testfd, offset, 0);
	if (write(testfd, datap, count) != count) {
		perror("tlock: testfile write");
		testexit(1);
	}
#ifdef LARGE_LOCKS
	comment("Wrote '%s' to testfile [ %lld, %d ].",
		datap, (void *)offset, (void *)count);
#else
	comment("Wrote '%s' to testfile [ %ld, %d ].",
		datap, (void *)offset, (void *)count);
#endif
}

void
read_testfile(test, sec, datap, offset, count, pass, fail)
	int test;
	int sec;
	char *datap;
	off_t offset;
	unsigned int count;
	int pass;
	int fail;
{
	int result;
	char array[64];		/* there's a limit to this... */

	(void) lseek(testfd, offset, 0);
	if (count > sizeof (array))
		count = sizeof (array);
	if ((result = read(testfd, array, count)) != count) {
		perror("tlock: testfile read");
		testexit(1);
	}
#ifdef LARGE_LOCKS
	comment("Read '%s' from testfile [ %lld, %d ].",
		datap, (void *)offset, (void *)count);
#else
	comment("Read '%s' from testfile [ %ld, %d ].",
		datap, (void *)offset, (void *)count);
#endif
	array[count] = '\0';
	if (strncmp(datap, array, count) != 0) {
		comment("**** Test expected '%s', read '%s'.",
			datap, array);
		result = UNEQUAL;
	} else {
		result = EQUAL;
	}
	report(test, sec, "COMPARE", offset, (off_t)count, pass, result, fail);
}

void
testdup2(fd1, fd2)
	int fd1;
	int fd2;
{

	if (dup2(fd1, fd2) < 0) {
		perror("tlock: dup2");
		testexit(1);
	}
}

void
testtruncate()
{

	comment("Truncated testfile.");
	if (ftruncate(testfd, (off_t)0) < 0) {
		perror("tlock: ftruncate");
		testexit(1);
	}
}

#ifdef MMAP
/*
 * Try to mmap testfile.  It's not necessarily a fatal error if the request
 * fails.  Returns 0 on success and sets mappedaddr to the mapped address.
 * Returns an errno value on failure.
 */
int
testmmap()
{
	mappedaddr = mmap(0, mappedlen, PROT_READ | PROT_WRITE, MAP_SHARED,
			testfd, (off_t)0);
	if (mappedaddr == (caddr_t)-1)
		return(errno);
	return(0);
}

void
testmunmap()
{
	comment("unmap testfile.");
	if (munmap(mappedaddr, mappedlen) < 0) {
		perror("Can't unmap testfile.");
		testexit(1);
	}
	mappedaddr = (caddr_t)0xdeadbeef;
}
#endif /* MMAP */

void
test(num, sec, func, offset, length, pass, fail)
	int num;
	int sec;
	int func;
	off_t offset;
	off_t length;
	int pass;
	int fail;
{
	int result;

	(void) lseek(testfd, offset, 0);
	if ((result = lockf(testfd, func, length)) != 0) {
		if (result != -1) {
			fprintf(stderr, "tlock: lockf() returned %d.\n",
				result);
			testexit(1);
		}
		result = errno;
	}
	report(num, sec, tfunstr(func), offset, length, pass, result, fail);
}

void
rate(cnt)
	int cnt;
{
	int i;
	long beg;
	long end;
#if !defined(OSF1) && !defined(HPUX) && !defined(SOLARIS2X)
	extern long times();
#endif
	struct tms tms;
	long delta;

	beg = times(&tms);
	for (i = 0; i < cnt; i++) {
		if ((lockf(testfd, F_LOCK, 1) != 0) ||
		    (lockf(testfd, F_ULOCK, 1) != 0)) {
			fprintf(stderr, "tlock: rate error=%d.\n", errno);
			tstfail++;
			break;
		}
	}
	/* See makefile: Sun must use SysV times()! */
	end = times(&tms);
	delta = ((end - beg) * 1000) / HZ;
	if (delta == 0) {
		fprintf(stderr, "tlock: rate time=0.\n");
		return;
	}
	comment("Performed %d lock/unlock cycles in %d msecs. [%d lpm].",
		(void *)i, (void *)delta, (void *)((i * 120000) / delta));
	fflush(stdout);
}

void
test1()
{

	if (who == PARENT) {
		parentwait();
		open_testfile(OPENFLAGS, OPENMODES);
		header(1, "Test regions of an unlocked file.");
		test(1, 1, F_TEST, (off_t)0, (off_t)1, PASS, FATAL);
		test(1, 2, F_TEST, (off_t)0, (off_t)END, PASS, FATAL);
		test(1, 3, F_TEST, (off_t)0, (off_t)MAXEOF, PASS, FATAL);
		test(1, 4, F_TEST, (off_t)1, (off_t)1, PASS, FATAL);
		test(1, 5, F_TEST, (off_t)1, (off_t)END, PASS, FATAL);
		test(1, 6, F_TEST, (off_t)1, (off_t)MAXEOF, PASS, FATAL);
		test(1, 7, F_TEST, (off_t)MAXEOF, (off_t)1, PASS, FATAL);
		test(1, 8, F_TEST, (off_t)MAXEOF, (off_t)END, PASS, FATAL);
#ifdef LF_SUMMIT
		test(1, 9, F_TEST, (off_t)MAXEOF, (off_t)MAXEOF, EOVERFLOW,
			WARN);
#else
		test(1, 9, F_TEST, (off_t)MAXEOF, (off_t)MAXEOF, EINVAL, WARN);
#endif
		close_testfile(DO_UNLINK);
		childfree(0);
	} else {
		parentfree(0);
		childwait();
	}
}

void
test2()
{

	if (who == PARENT) {
		parentwait();
		header(2, "Try to lock the whole file.");
		open_testfile(OPENFLAGS, OPENMODES);
		test(2, 0, F_TLOCK, (off_t)0, (off_t)END, PASS, FATAL);
		childfree(0);
		parentwait();
		test(2, 10, F_ULOCK, (off_t)0, (off_t)END, PASS, FATAL);
		close_testfile(DO_UNLINK);
	} else {
		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
#ifdef SVR4
		test(2, 1, F_TEST, (off_t)0, (off_t)1, EAGAIN, WARN);
#else
		test(2, 1, F_TEST, (off_t)0, (off_t)1, EACCES, FATAL);
#endif
		test(2, 2, F_TEST, (off_t)0, (off_t)END, EAGAIN, WARN);
#ifdef SVR4
		test(2, 3, F_TEST, (off_t)0, (off_t)MAXEOF, EAGAIN, WARN);
		test(2, 4, F_TEST, (off_t)1, (off_t)1, EAGAIN, WARN);
		test(2, 5, F_TEST, (off_t)1, (off_t)END, EAGAIN, WARN);
		test(2, 6, F_TEST, (off_t)1, (off_t)MAXEOF, EAGAIN, WARN);
		test(2, 7, F_TEST, (off_t)MAXEOF, (off_t)1, EAGAIN, WARN);
		test(2, 8, F_TEST, (off_t)MAXEOF, (off_t)END, EAGAIN, WARN);
#else
		test(2, 3, F_TEST, (off_t)0, (off_t)MAXEOF, EACCES, FATAL);
		test(2, 4, F_TEST, (off_t)1, (off_t)1, EACCES, FATAL);
		test(2, 5, F_TEST, (off_t)1, (off_t)END, EACCES, FATAL);
		test(2, 6, F_TEST, (off_t)1, (off_t)MAXEOF, EACCES, FATAL);
		test(2, 7, F_TEST, (off_t)MAXEOF, (off_t)1, EACCES, WARN);
		test(2, 8, F_TEST, (off_t)MAXEOF, (off_t)END, EACCES, WARN);
#endif
#ifdef LF_SUMMIT
		test(2, 9, F_TEST, (off_t)MAXEOF, (off_t)MAXEOF, EOVERFLOW,
			WARN);
#else
		test(2, 9, F_TEST, (off_t)MAXEOF, (off_t)MAXEOF, EINVAL, WARN);
#endif
		close_testfile(DO_UNLINK);
		parentfree(0);
	}
}

void
test3()
{

	if (who == PARENT) {
		parentwait();
		header(3, "Try to lock just the 1st byte.");
		open_testfile(OPENFLAGS, OPENMODES);
		test(3, 0, F_TLOCK, (off_t)0, (off_t)1, PASS, FATAL);
		childfree(0);
		parentwait();
		test(3, 5, F_ULOCK, (off_t)0, (off_t)1, PASS, FATAL);
		close_testfile(DO_UNLINK);
	} else {
		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
#ifdef SVR4
		test(3, 1, F_TEST, (off_t)0, (off_t)1, EAGAIN, WARN);
		test(3, 2, F_TEST, (off_t)0, (off_t)END, EAGAIN, WARN);
#else
		test(3, 1, F_TEST, (off_t)0, (off_t)1, EACCES, FATAL);
		test(3, 2, F_TEST, (off_t)0, (off_t)END, EACCES, FATAL);
#endif
		test(3, 3, F_TEST, (off_t)1, (off_t)1, PASS, FATAL);
		test(3, 4, F_TEST, (off_t)1, (off_t)END, PASS, FATAL);
		close_testfile(DO_UNLINK);
		parentfree(0);
	}
}

void
test4()
{

	if (who == PARENT) {
		parentwait();
		header(4, "Try to lock the 2nd byte, test around it.");
		open_testfile(OPENFLAGS, OPENMODES);
		test(4, 0, F_TLOCK, (off_t)1, (off_t)1, PASS, FATAL);
		childfree(0);
		parentwait();
		test(4, 10, F_ULOCK, (off_t)1, (off_t)1, PASS, FATAL);
		close_testfile(DO_UNLINK);
	} else {
		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
		test(4, 1, F_TEST, (off_t)0, (off_t)1, PASS, FATAL);
#ifdef SVR4
		test(4, 2, F_TEST, (off_t)0, (off_t)2, EAGAIN, WARN);
		test(4, 3, F_TEST, (off_t)0, (off_t)END, EAGAIN, WARN);
		test(4, 4, F_TEST, (off_t)1, (off_t)1, EAGAIN, WARN);
		test(4, 5, F_TEST, (off_t)1, (off_t)2, EAGAIN, WARN);
		test(4, 6, F_TEST, (off_t)1, (off_t)END, EAGAIN, WARN);
#else
		test(4, 2, F_TEST, (off_t)0, (off_t)2, EACCES, FATAL);
		test(4, 3, F_TEST, (off_t)0, (off_t)END, EACCES, FATAL);
		test(4, 4, F_TEST, (off_t)1, (off_t)1, EACCES, FATAL);
		test(4, 5, F_TEST, (off_t)1, (off_t)2, EACCES, FATAL);
		test(4, 6, F_TEST, (off_t)1, (off_t)END, EACCES, FATAL);
#endif
		test(4, 7, F_TEST, (off_t)2, (off_t)1, PASS, FATAL);
		test(4, 8, F_TEST, (off_t)2, (off_t)2, PASS, FATAL);
		test(4, 9, F_TEST, (off_t)2, (off_t)END, PASS, FATAL);
		close_testfile(DO_UNLINK);
		parentfree(0);
	}
}

void
test5()
{

	if (who == PARENT) {
		parentwait();
		header(5, "Try to lock 1st and 2nd bytes, test around them.");
		open_testfile(OPENFLAGS, OPENMODES);
		test(5, 0, F_TLOCK, (off_t)0, (off_t)1, PASS, FATAL);
		test(5, 1, F_TLOCK, (off_t)2, (off_t)1, PASS, FATAL);
		childfree(0);
		parentwait();
		test(5, 14, F_ULOCK, (off_t)0, (off_t)1, PASS, FATAL);
		test(5, 15, F_ULOCK, (off_t)2, (off_t)1, PASS, FATAL);
		close_testfile(DO_UNLINK);
	} else {
		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
#ifdef SVR4
		test(5, 2, F_TEST, (off_t)0, (off_t)1, EAGAIN, WARN);
		test(5, 3, F_TEST, (off_t)0, (off_t)2, EAGAIN, WARN);
		test(5, 4, F_TEST, (off_t)0, (off_t)END, EAGAIN, WARN);
#else
		test(5, 2, F_TEST, (off_t)0, (off_t)1, EACCES, FATAL);
		test(5, 3, F_TEST, (off_t)0, (off_t)2, EACCES, FATAL);
		test(5, 4, F_TEST, (off_t)0, (off_t)END, EACCES, FATAL);
#endif
		test(5, 5, F_TEST, (off_t)1, (off_t)1, PASS, FATAL);
#ifdef SVR4
		test(5, 6, F_TEST, (off_t)1, (off_t)2, EAGAIN, WARN);
		test(5, 7, F_TEST, (off_t)1, (off_t)END, EAGAIN, WARN);
		test(5, 8, F_TEST, (off_t)2, (off_t)1, EAGAIN, WARN);
		test(5, 9, F_TEST, (off_t)2, (off_t)2, EAGAIN, WARN);
		test(5, 10, F_TEST, (off_t)2, (off_t)END, EAGAIN, WARN);
#else
		test(5, 6, F_TEST, (off_t)1, (off_t)2, EACCES, FATAL);
		test(5, 7, F_TEST, (off_t)1, (off_t)END, EACCES, FATAL);
		test(5, 8, F_TEST, (off_t)2, (off_t)1, EACCES, FATAL);
		test(5, 9, F_TEST, (off_t)2, (off_t)2, EACCES, FATAL);
		test(5, 10, F_TEST, (off_t)2, (off_t)END, EACCES, FATAL);
#endif
		test(5, 11, F_TEST, (off_t)3, (off_t)1, PASS, FATAL);
		test(5, 12, F_TEST, (off_t)3, (off_t)2, PASS, FATAL);
		test(5, 13, F_TEST, (off_t)3, (off_t)END, PASS, FATAL);
		close_testfile(DO_UNLINK);
		parentfree(0);
	}
}

void
test6()
{
#ifdef LARGE_LOCKS
	unsigned long long maxplus1 = (unsigned long long)MAXEOF + 1;
#else
	unsigned long maxplus1 = (unsigned long)MAXEOF + 1;
#endif

	if (who == PARENT) {
		parentwait();
		header(6, "Try to lock the MAXEOF byte.");
		open_testfile(OPENFLAGS, OPENMODES);
		test(6, 0, F_TLOCK, (off_t)MAXEOF, (off_t)1, PASS, FATAL);
		childfree(0);
		parentwait();
		test(6, 10, F_ULOCK, (off_t)MAXEOF, (off_t)1, PASS, FATAL);
		close_testfile(DO_UNLINK);
	} else {
		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
		test(6, 1, F_TEST, (off_t)MAXEOF - 1, (off_t)1, PASS, FATAL);
#ifdef SVR4
		test(6, 2, F_TEST, (off_t)MAXEOF - 1, (off_t)2, EAGAIN, WARN);
		test(6, 3, F_TEST, (off_t)MAXEOF - 1, (off_t)END, EAGAIN, WARN);
		test(6, 4, F_TEST, (off_t)MAXEOF, (off_t)1, EAGAIN, WARN);
#else
		test(6, 2, F_TEST, (off_t)MAXEOF - 1, (off_t)2, EACCES, FATAL);
		test(6, 3, F_TEST, (off_t)MAXEOF - 1, (off_t)END, EACCES, WARN);
		test(6, 4, F_TEST, (off_t)MAXEOF, (off_t)1, EACCES, FATAL);
#endif
#ifdef LF_SUMMIT
		test(6, 5, F_TEST, (off_t)MAXEOF, (off_t)2, EOVERFLOW, WARN);
#else
		test(6, 5, F_TEST, (off_t)MAXEOF, (off_t)2, EINVAL, WARN);
#endif
#ifdef SVR4
		test(6, 6, F_TEST, (off_t)MAXEOF, (off_t)END, EAGAIN, WARN);
		test(6, 7, F_TEST, (off_t)maxplus1, (off_t)1, EAGAIN, WARN);
#else
		test(6, 6, F_TEST, (off_t)MAXEOF, (off_t)END, EACCES, FATAL);
		test(6, 7, F_TEST, (off_t)maxplus1, (off_t)1, EACCES, WARN);
#endif
#if defined(IRIX) && !defined(LARGE_LOCKS)
		test(6, 8, F_TEST, (off_t)maxplus1, (off_t)2, EINVAL, WARN);
#else
#ifdef LF_SUMMIT
		test(6, 8, F_TEST, (off_t)maxplus1, (off_t)2, EOVERFLOW, FATAL);
#else
		test(6, 8, F_TEST, (off_t)maxplus1, (off_t)2, EINVAL, FATAL);
#endif
#endif
#ifdef SVR4
		test(6, 9, F_TEST, (off_t)maxplus1, (off_t)END, EAGAIN, WARN);
#else
		test(6, 9, F_TEST, (off_t)maxplus1, (off_t)END, EACCES, WARN);
#endif
		close_testfile(DO_UNLINK);
		parentfree(0);
	}
}

void
test7()
{

	if (who == PARENT) {
		parentwait();
		header(7, "Test parent/child mutual exclusion.");
		open_testfile(OPENFLAGS, OPENMODES);
		test(7, 0, F_TLOCK, (off_t)0, (off_t)4, PASS, FATAL);
		write_testfile("aaaa", (off_t)0, 4);
		comment("Now free child to run, should block on lock.");
		childfree(wait_time);
		comment("Check data in file to insure child blocked.");
		read_testfile(7, 1, "aaaa", (off_t)0, 4, EQUAL, FATAL);
		comment("Now unlock region so child will unblock.");
		test(7, 2, F_ULOCK, (off_t)0, (off_t)4, PASS, FATAL);
		comment("Now try to regain lock, parent should block.");
		test(7, 5, F_LOCK, (off_t)0, (off_t)4, PASS, FATAL);
		comment("Check data in file to insure child unblocked.");
		read_testfile(7, 6, "bbbb", (off_t)0, 4, EQUAL, FATAL);
		test(7, 7, F_ULOCK, (off_t)0, (off_t)4, PASS, FATAL);
		close_testfile(DO_UNLINK);
		childfree(0);
	} else {
		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
		test(7, 3, F_LOCK, (off_t)0, (off_t)4, PASS, FATAL);
		write_testfile("bbbb", (off_t)0, 4);
		test(7, 4, F_ULOCK, (off_t)0, (off_t)4, PASS, FATAL);
		close_testfile(DO_UNLINK);
		childwait();
	}
}

void
test8()
{

	if (who == PARENT) {
		parentwait();
		header(8, "Rate test performing lock/unlock cycles.");
		open_testfile(OPENFLAGS, OPENMODES);
		rate(ratecount);
		close_testfile(DO_UNLINK);
		childfree(0);
	} else {
		parentfree(0);
		childwait();
	}
}

void
test9()
{

	if (who == PARENT) {
		parentwait();
		header(9, "Test mandatory locking (LAI NFS client).");
		open_testfile(OPENFLAGS, MANDMODES);
		test(9, 0, F_TLOCK, (off_t)0, (off_t)4, PASS, FATAL);
		write_testfile("aaaa", (off_t)0, 4);
		comment("Now free child to run, should block on write.");
		childfree(wait_time);
		comment("Check data in file before child writes.");
		read_testfile(9, 1, "aaaa", (off_t)0, 4, EQUAL, FATAL);
		comment("Now unlock region so child will write.");
		test(9, 2, F_ULOCK, (off_t)0, (off_t)4, PASS, FATAL);
		comment("Check data in file after child writes.");
		read_testfile(9, 3, "bbbb", (off_t)0, 4, EQUAL, FATAL);
		close_testfile(DO_UNLINK);
		childfree(0);
	} else {
		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
		write_testfile("bbbb", (off_t)0, 4);
		close_testfile(DO_UNLINK);
		childwait();
	}
}

void
test10()
{

	if (who == PARENT) {
		parentwait();
		header(10, "Make sure a locked region is split properly.");
		open_testfile(OPENFLAGS, OPENMODES);
		test(10, 0, F_TLOCK, (off_t)0, (off_t)3, PASS, FATAL);
		test(10, 1, F_ULOCK, (off_t)1, (off_t)1, PASS, FATAL);
		childfree(0);
		parentwait();
		test(10, 6, F_ULOCK, (off_t)0, (off_t)1, PASS, FATAL);
		test(10, 7, F_ULOCK, (off_t)2, (off_t)1, PASS, FATAL);
		childfree(0);
		parentwait();
		test(10, 9, F_ULOCK, (off_t)0, (off_t)1, PASS, WARN);
		test(10, 10, F_TLOCK, (off_t)1, (off_t)3, PASS, WARN);
		test(10, 11, F_ULOCK, (off_t)2, (off_t)1, PASS, WARN);
		childfree(0);
		parentwait();
		close_testfile(DO_UNLINK);
	} else {
		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
#ifdef SVR4
		test(10, 2, F_TEST, (off_t)0, (off_t)1, EAGAIN, WARN);
		test(10, 3, F_TEST, (off_t)2, (off_t)1, EAGAIN, WARN);
#else
		test(10, 2, F_TEST, (off_t)0, (off_t)1, EACCES, WARN);
		test(10, 3, F_TEST, (off_t)2, (off_t)1, EACCES, WARN);
#endif
		test(10, 4, F_TEST, (off_t)3, (off_t)END, PASS, WARN);
		test(10, 5, F_TEST, (off_t)1, (off_t)1, PASS, WARN);
		parentfree(0);
		childwait();
		test(10, 8, F_TEST, (off_t)0, (off_t)3, PASS, WARN);
		parentfree(0);
		childwait();
#ifdef SVR4
		test(10, 12, F_TEST, (off_t)1, (off_t)1, EAGAIN, WARN);
		test(10, 13, F_TEST, (off_t)3, (off_t)1, EAGAIN, WARN);
#else
		test(10, 12, F_TEST, (off_t)1, (off_t)1, EACCES, WARN);
		test(10, 13, F_TEST, (off_t)3, (off_t)1, EACCES, WARN);
#endif
		test(10, 14, F_TEST, (off_t)4, (off_t)END, PASS, WARN);
		test(10, 15, F_TEST, (off_t)2, (off_t)1, PASS, WARN);
		test(10, 16, F_TEST, (off_t)0, (off_t)1, PASS, WARN);
		close_testfile(DO_UNLINK);
		parentfree(0);
	}
}

void
test11()
{
	int dupfd;
	char data[] = "123456789abcdef";

	if (who == PARENT) {
		parentwait();
		header(11, "Make sure close() releases the process's locks.");
		open_testfile(OPENFLAGS, OPENMODES);
		dupfd = dup(testfd);
		test(11, 0, F_TLOCK, (off_t)0, (off_t)0, PASS, FATAL);
		close_testfile(JUST_CLOSE);
		childfree(0);

		parentwait();
		testdup2(dupfd, testfd);
		test(11, 3, F_TLOCK, (off_t)29, (off_t)1463, PASS, FATAL);
		test(11, 4, F_TLOCK, (off_t)0x2000, (off_t)87, PASS, FATAL);
		close_testfile(JUST_CLOSE);
		childfree(0);

		parentwait();
		testdup2(dupfd, testfd);
		write_testfile(data, (off_t)0, sizeof (data));
		test(11, 7, F_TLOCK, (off_t)0, (off_t)0, PASS, FATAL);
		write_testfile(data, (off_t)(sizeof (data)-3), sizeof (data));
		close_testfile(JUST_CLOSE);
		childfree(0);

		parentwait();
		testdup2(dupfd, testfd);
		write_testfile(data, (off_t)0, sizeof (data));
		test(11, 10, F_TLOCK, (off_t)0, (off_t)0, PASS, FATAL);
		testtruncate();
		close_testfile(JUST_CLOSE);
		childfree(0);

		parentwait();
		close(dupfd);
		close_testfile(DO_UNLINK);
	} else {
		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
		test(11, 1, F_TLOCK, (off_t)0, (off_t)0, PASS, FATAL);
		test(11, 2, F_ULOCK, (off_t)0, (off_t)0, PASS, FATAL);
		parentfree(0);

		childwait();
		test(11, 5, F_TLOCK, (off_t)0, (off_t)0, PASS, FATAL);
		test(11, 6, F_ULOCK, (off_t)0, (off_t)0, PASS, FATAL);
		parentfree(0);

		childwait();
		test(11, 8, F_TLOCK, (off_t)0, (off_t)0, PASS, FATAL);
		test(11, 9, F_ULOCK, (off_t)0, (off_t)0, PASS, FATAL);
		parentfree(0);

		childwait();
		test(11, 11, F_TLOCK, (off_t)0, (off_t)0, PASS, FATAL);
		test(11, 12, F_ULOCK, (off_t)0, (off_t)0, PASS, FATAL);
		close_testfile(DO_UNLINK);
		parentfree(0);
	}
}

void
test12()
{
	if (who == PARENT) {
		pid_t target;

		parentwait();
		header(12, "Signalled process should release locks.");
		open_testfile(OPENFLAGS, OPENMODES);
		childfree(0);

		parentwait();
		(void) lseek(testfd, (off_t)0, 0);
		if (read(testfd, &target, sizeof (target)) !=
		    sizeof (target)) {
			perror("can't read pid to kill");
			testexit(1);
		}
		kill(target, SIGINT);
		comment("Killed child process.");
		sleep(wait_time);
		test(12, 1, F_TLOCK, (off_t)0, (off_t)0, PASS, FATAL);
		childfree(0);
		close_testfile(DO_UNLINK);
	} else {
		pid_t subchild;

		parentfree(0);
		childwait();
		open_testfile(OPENFLAGS, OPENMODES);
		/*
		 * Create a subprocess to obtain a lock and get killed.  If
		 * the parent kills the regular child, tlock will stop
		 * after the first pass.
		 */
		subchild = fork();
		if (subchild < 0) {
			perror("can't fork off subchild");
			testexit(1);
		}
		/*
		 * Record the pid of the subprocess and wait for the parent
		 * to tell the child that the test is done.  Note that the
		 * child and subchild share the file offset; keep this in
		 * mind if you change this test.
		 */
		if (subchild > 0) {
			/* original child */
			sleep(wait_time);
			(void) lseek(testfd, (off_t)0, 0);
			if (write(testfd, &subchild, sizeof (subchild)) !=
			    sizeof (subchild)) {
				perror("can't record pid to kill");
				kill(subchild, SIGINT);
				testexit(1);
			}
			parentfree(0);
			childwait();
			close_testfile(DO_UNLINK);
		} else {
			/* subchild */
			signal(SIGINT, SIG_DFL);
			test(12, 0, F_TLOCK, (off_t)0, (off_t)0, PASS, FATAL);
			for (;;)
				sleep(1);
			/* NOTREACHED */
		}
	}
}

#ifdef MMAP
void
test13()
{
	if (who == PARENT) {
		int lock1err;
		int err;

		parentwait();
		open_testfile(OPENFLAGS, OPENMODES);
		header(13, "Check locking and mmap semantics.");

		/*
		 * Can a file be locked and mapped at same time?
		 */
		test(13, 0, F_TLOCK, (off_t)mappedlen - 2, (off_t)END, PASS,
			FATAL);
		lock1err = testmmap();
		report(13, 1, "mmap", (off_t)0, (off_t)mappedlen, EAGAIN,
			lock1err, WARN);
		test(13, 2, F_ULOCK, (off_t)0, (off_t)END, PASS, FATAL);
		if (lock1err == 0)
			testmunmap();

		/*
		 * Does the order of lock/mmap matter?  This also verifies
		 * that releasing the lock makes the file mappable again.
		 */
		err = testmmap();
		report(13, 3, "mmap", (off_t)0, (off_t)mappedlen, PASS, err,
			FATAL);
		test(13, 4, F_TLOCK, (off_t)mappedlen - 2, (off_t)END,
			lock1err, WARN);
		close_testfile(DO_UNLINK);

		childfree(0);
	} else {
		parentfree(0);
		childwait();
	}
}
#endif /* MMAP */

void
runtests()
{

	if (DO_TEST(1)) {
		test1();
	}
	if (DO_TEST(2)) {
		test2();
	}
	if (DO_TEST(3)) {
		test3();
	}
	if (DO_TEST(4)) {
		test4();
	}
	if (DO_TEST(5)) {
		test5();
	}
	if (DO_TEST(6)) {
		test6();
	}
	if (DO_TEST(7)) {
		test7();
	}
	if (DO_RATE(8)) {
		test8();
	}
	if (DO_MAND(9)) {
		test9();
	}
	if (DO_TEST(10)) {
		test10();
	}
	if (DO_TEST(11)) {
		test11();
	}
	if (DO_TEST(12)) {
		test12();
	}
#ifdef MMAP
	if (DO_TEST(13)) {
		test13();
	}
#endif
}

/*
 * Main record locking test loop.
 */
main(argc, argv)
	int argc;
	char **argv;
{
	int c;
	extern int optind;
	extern char *optarg;
	int errflg = 0;

	passcnt = 1;	/* default, test for 1 pass */

	while ((c = getopt(argc, argv, "p:t:rmw:")) != -1) {
		switch (c) {
		case 'p':
			sscanf(optarg, "%d", &passcnt);
			break;
		case 't':
			sscanf(optarg, "%d", &testnum);
			break;
		case 'r':
			ratetest++;
			break;
		case 'm':
			mandtest++;
			break;
		case 'w':
			sscanf(optarg, "%d", &wait_time);
			break;
		default:
			errflg++;
		}
	}
	if (errflg) {
		fprintf(stderr,
"usage: tlock [-p passcnt] [-t testnum] [-r] [-m] [-w wait_time] [dirpath]\n");
		exit(2);
	}
	if (optind < argc) {
		filepath = argv[optind];
	}
	initialize();

	/*
	 * Fork child...
	 */
	if ((childpid = fork()) == 0) {
		who = CHILD;
		signal(SIGINT, parentsig);
	} else {
		who = PARENT;
		signal(SIGINT, childsig);
		signal(SIGCLD, childdied);
	}

	/*
	 * ...and run the tests for count passes.
	 */
	for (passnum = 1; passnum <= passcnt; passnum++) {
		runtests();
		if (who == CHILD) {
			childwait();
			testreport(0);
		} else {
			testreport(0);
			childfree(0);
		}
	}
	if (who == CHILD) {
		childwait();
	} else {
		signal(SIGCLD, SIG_DFL);
		childfree(0);
	}
	testexit(0);
	/* NOTREACHED */
}
