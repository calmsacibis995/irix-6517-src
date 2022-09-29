/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.14 $"

#include "sys/types.h"
#include "limits.h"
#include "unistd.h"
#include "stdlib.h"
#include "signal.h"
#include "bstring.h"
#include "stdio.h"
#include "sys/times.h"
#include "errno.h"
#include "sys/prctl.h"
#include "sys/wait.h"
#include "stress.h"
#include "getopt.h"
#include "ulocks.h"
#include "dlfcn.h"

/*
 * test sproc create/destroy rate
 */
char *Cmd;
volatile int noexit = 0;
int touchstk;
int nocheck = 0;
long pagesz;
caddr_t lowstack;	/* guess at lowest possible stack any thread should get */
int verbose;
int spinonerror = 0;
int nprocspl;		/* # procs per loop */
int syncup;
pid_t ppid;
int exitsig;
int forktest;
volatile int stopforktest;
void usage(void);
void callem(void);
extern void slave(void *, size_t);

/* ARGSUSED */
void
segcatch(int sig, int code, struct sigcontext *sc)
{
	if (exitsig)
		exit(0);
	fprintf(stderr, "%s:%d caught signal %d at pc 0x%llx addr 0x%llx\n",
			Cmd, get_pid(), sig, sc->sc_pc, sc->sc_badvaddr);
	if (forktest)
		for(;;);
	abort();
	/* NOTREACHED */
}

int
main(int argc, char **argv)
{
	struct tms tm;
	register int c, i, j;
	clock_t start, ti;
	int nprocs;		/* total procs created */
	int doloop;		/* # procs this loop */
	pid_t *pids;
	pid_t pid;
	size_t stksize, pstksize, totstksz;
	auto int status;
	int dowait = 0;
	int nolibc = 0;
	int betweennap = 0;
	int nocatch = 0;

	Cmd = errinit(argv[0]);
	pstksize = stksize = prctl(PR_GETSTACKSIZE);
	nprocspl = 20;
	pagesz = sysconf(_SC_PAGESIZE);
	ppid = get_pid();
	while ((c = getopt(argc, argv, "cf:uea:hnlvws:tp:")) != EOF) {
		switch (c) {
		case 'f':
			forktest = atoi(optarg);
			break;
		case 'c':
			nocatch = 1;
			break;
		case 'e':
			exitsig = 1;
			break;
		case 'u':
			syncup = 1;
			break;
		case 'a':
			betweennap = atoi(optarg);
			break;
		case 'h':
			spinonerror = 1;
			break;
		case 'l':
			nolibc = 1;
			break;
		case 'n':
			nocheck = 1;
			break;
		case 'w':
			dowait = 1;
			break;
		case 's':
			stksize = atoi(optarg) * 1024;
			break;
		case 't':
			/* touch bottom of stack */
			touchstk = 1;
			break;
		case 'p':
			nprocspl = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
			break;
		}
	}

	if (optind >= argc)
		usage();
	nprocs = atoi(argv[optind]);
	printf("%s:Creating %d processes (%d at a time) max stack size:%d Kb\n",
		Cmd, nprocs, nprocspl, stksize/1024);

	if (!nocatch) {
		sigset(SIGSEGV, segcatch);
		sigset(SIGBUS, segcatch);
	}
	if (!nolibc) {
		if (forktest) {
			if (usconfig(CONF_INITUSERS, 2*(nprocspl+1)) < 0)
				errprintf(1, "INITUSERS");
		} else {
			if (usconfig(CONF_INITUSERS, nprocspl+1) < 0)
				errprintf(1, "INITUSERS");
		}
	}

	/*
	 * this tries to compute a lower stack bounds - to attempt
	 * to check that we are properly recycling stack addresses
	 * Obviously there are lots of input options that would
	 * make these calculations overflow
	 * To try to figure this out we compute stuff in pages and
	 * see if that is greater than MAXLONG/pagesz
	 */
	totstksz = nprocspl * ((stksize + pagesz) / pagesz);
	totstksz += (pstksize / pagesz);
	if (totstksz > (LONG_MAX / pagesz))
		lowstack = 0;
	else
		lowstack = (caddr_t)&argv - (totstksz * pagesz);

	pids = calloc(nprocspl, sizeof(pid_t));
	start = times(&tm);

	for (j = nprocs; j > 0; j -= nprocspl) {
		stopforktest = 0;
		bzero(pids, nprocspl * (int)sizeof(pid_t));
		if (verbose)
			printf("%s:%d procs remaining\n", Cmd, j);
		if (dowait)
			noexit = 1;
		if (j > nprocspl)
			doloop = nprocspl;
		else
			doloop = j;

		for (i = 0; i < doloop; i++) {
tryagain:
			if ((pids[i] = sprocsp(slave,
					PR_SALL | (nolibc ? PR_NOLIBC : 0),
					(void *)(((long)j << 16) | (long)i),
					NULL, stksize)) < 0) {
				if (errno == ENOMEM && (stksize > (1024*16))) {
					stksize /= 2;
					if (verbose)
						printf("%s:sproc failed reducing stack size to %d\n", Cmd, stksize);
					goto tryagain;
				} else
					perror("sproc failed");
			}
			if (betweennap)
				sginap(betweennap);
			if (syncup)
				blockproc(get_pid());
		}
		noexit = 0;
		/* wait for all threads to finish */
		if (forktest) {
			sginap(forktest);
			stopforktest = 1;
		} else if (exitsig) {
			for (i = 0; i < doloop; i++) {
				if (pids[i] > 0)
					kill(pids[i], SIGBUS);
			}
		}
		for (;;) {
			pid = wait(&status);
			if (pid < 0 && errno == ECHILD)
				break;
			else if ((pid >= 0) && (status & 0xff))
				fprintf(stderr, "proc %d died of signal %d\n",
					pid, status & ~0200);
		}
	}

	ti = times(&tm) - start;
	if (verbose)
		printf("start:%x time:%x\n", start, times(&tm));
	ti = (ti*1000)/(clock_t)CLK_TCK;
	printf("%s:time for %d sprocs %d mS or %d uS per create/destroy\n",
		Cmd, nprocs, ti, (ti*1000)/(clock_t)nprocs);
	return 0;
}

void
slave(void *arg, size_t stksize)
{
	int looper;
	unsigned snum = (unsigned)((__psunsigned_t)arg & 0xffff);
	pid_t pid;

	if (syncup)
		unblockproc(ppid);
	if (verbose)
		printf("%s:pid %d stack @ 0x%p\n",
			Cmd, get_pid(), &arg);
	if (touchstk) {
		caddr_t stkbot;

		/* round up to page, then subtract stksize */
		stkbot = (caddr_t)((((size_t)&arg + pagesz - 1) / pagesz) * pagesz);
		stkbot -= stksize;
		if (*stkbot != 0)
			errprintf(0, "Bottom of stack not 0!");
			/* NOTREACHED */
	}

	if (!nocheck)
		if ((caddr_t)&arg < lowstack) {
			errprintf(2, "pid %d stack @ 0x%p low @ 0x%p",
				get_pid(), &arg, lowstack);
			if (spinonerror) {
				for (;;)
					sginap(100);
			} else
				abort();
		}

	/*
	 * for forktest, odd slaves will fork/exit while even ones
	 * do dlopen/close. This is to test the _rld_interface(SPROC_FINI)
	 * option. Note that we need to run non-quickstart.
	 */
	if (forktest && ((snum % 2) == 0)) {
		if (verbose)
			printf("thread %d pid %d forking\n", snum, get_pid());
		while (stopforktest == 0) {
			if ((pid = fork()) == 0) {
				/* child - assumption that the call to exit will
				 * get into LTR in rld
				 */
				exit(0);
			} else {
				while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
					;
			}
		}
		exit(0);
	}
	/*
	 * don't want to exit since we will be getting a signal! (in the case
	 * of exitsig)
	 */
	while ((exitsig || forktest) && (stopforktest == 0)) {
		void *tlonfs_lib;

		if (verbose)
			printf("thread %d pid %d dlopen'ing\n", snum, get_pid());
		if ((tlonfs_lib = sgidladd("./lonfs.so", RTLD_LAZY)) == NULL) {
			errprintf(ERR_EXIT, "open of lonfs.so failed");
			/* NOTREACHED */
		}
		callem();
		dlclose(tlonfs_lib);
	}

	/* total heuristic!  - we need some basic wait time
	 * or we totally oblierate the schedular
	 */
	looper = nprocspl / 2;
	while (noexit) {
		/*
		 * spining alot really messes up tests with large (>500)
		 * threads
		 */
		sginap(looper);
		if (looper >= nprocspl/2)
			looper /= 2;
	}
	exit(0);
}

void
usage(void)
{
	fprintf(stderr, "Usage:%s [-lvwtnf] [-s #][-p #] totalprocs\n", Cmd);
	fprintf(stderr, "\t-l\tdo not init libc arena\n");
	fprintf(stderr, "\t-v\tverbose\n");
	fprintf(stderr, "\t-w\twait for all creations before slaves exit\n");
	fprintf(stderr, "\t-t\thave slaves touch bottom of stack\n");
	fprintf(stderr, "\t-n\tdon't check stack bounds\n");
	fprintf(stderr, "\t-f\tperform fork/dlopen test\n");
	fprintf(stderr, "\t-s#\tset each slaves stack size to 'n'Kb\n");
	fprintf(stderr, "\t-p#\tcreate 'n' slaves per loop\n");
	exit(-1);
}

extern void a0(void);
extern void a1(void);
extern void a2(void);
extern void a3(void);
extern void a4(void);
extern void a5(void);
extern void a6(void);
extern void a7(void);
extern void a8(void);
extern void a9(void);
extern void b0(void);
extern void b1(void);
extern void b2(void);
extern void b3(void);
extern void b4(void);
extern void b5(void);
extern void b6(void);
extern void b7(void);
extern void b8(void);
extern void b9(void);
extern void c0(void);
extern void c1(void);
extern void c2(void);
extern void c3(void);
extern void c4(void);
extern void c5(void);
extern void c6(void);
extern void c7(void);
extern void c8(void);
extern void c9(void);
extern void d0(void);
extern void d1(void);
extern void d2(void);
extern void d3(void);
extern void d4(void);
extern void d5(void);
extern void d6(void);
extern void d7(void);
extern void d8(void);
extern void d9(void);
extern void e0(void);
extern void e1(void);
extern void e2(void);
extern void e3(void);
extern void e4(void);
extern void e5(void);
extern void e6(void);
extern void e7(void);
extern void e8(void);
extern void e9(void);
extern void f0(void);
extern void f1(void);
extern void f2(void);
extern void f3(void);
extern void f4(void);
extern void f5(void);
extern void f6(void);
extern void f7(void);
extern void f8(void);
extern void f9(void);
extern void g0(void);
extern void g1(void);
extern void g2(void);
extern void g3(void);
extern void g4(void);
extern void g5(void);
extern void g6(void);
extern void g7(void);
extern void g8(void);
extern void g9(void);
extern void h0(void);
extern void h1(void);
extern void h2(void);
extern void h3(void);
extern void h4(void);
extern void h5(void);
extern void h6(void);
extern void h7(void);
extern void h8(void);
extern void h9(void);
extern void i0(void);
extern void i1(void);
extern void i2(void);
extern void i3(void);
extern void i4(void);
extern void i5(void);
extern void i6(void);
extern void i7(void);
extern void i8(void);
extern void i9(void);
extern void j0(void);
extern void j1(void);
extern void j2(void);
extern void j3(void);
extern void j4(void);
extern void j5(void);
extern void j6(void);
extern void j7(void);
extern void j8(void);
extern void j9(void);
extern void k0(void);
extern void k1(void);
extern void k2(void);
extern void k3(void);
extern void k4(void);
extern void k5(void);
extern void k6(void);
extern void k7(void);
extern void k8(void);
extern void k9(void);
extern void l0(void);
extern void l1(void);
extern void l2(void);
extern void l3(void);
extern void l4(void);
extern void l5(void);
extern void l6(void);
extern void l7(void);
extern void l8(void);
extern void l9(void);
extern void m0(void);
extern void m1(void);
extern void m2(void);
extern void m3(void);
extern void m4(void);
extern void m5(void);
extern void m6(void);
extern void m7(void);
extern void m8(void);
extern void m9(void);
extern void n0(void);
extern void n1(void);
extern void n2(void);
extern void n3(void);
extern void n4(void);
extern void n5(void);
extern void n6(void);
extern void n7(void);
extern void n8(void);
extern void n9(void);
extern void o0(void);
extern void o1(void);
extern void o2(void);
extern void o3(void);
extern void o4(void);
extern void o5(void);
extern void o6(void);
extern void o7(void);
extern void o8(void);
extern void o9(void);
extern void p0(void);
extern void p1(void);
extern void p2(void);
extern void p3(void);
extern void p4(void);
extern void p5(void);
extern void p6(void);
extern void p7(void);
extern void p8(void);
extern void p9(void);
extern void q0(void);
extern void q1(void);
extern void q2(void);
extern void q3(void);
extern void q4(void);
extern void q5(void);
extern void q6(void);
extern void q7(void);
extern void q8(void);
extern void q9(void);
extern void r0(void);
extern void r1(void);
extern void r2(void);
extern void r3(void);
extern void r4(void);
extern void r5(void);
extern void r6(void);
extern void r7(void);
extern void r8(void);
extern void r9(void);
extern void s0(void);
extern void s1(void);
extern void s2(void);
extern void s3(void);
extern void s4(void);
extern void s5(void);
extern void s6(void);
extern void s7(void);
extern void s8(void);
extern void s9(void);
extern void t0(void);
extern void t1(void);
extern void t2(void);
extern void t3(void);
extern void t4(void);
extern void t5(void);
extern void t6(void);
extern void t7(void);
extern void t8(void);
extern void t9(void);
extern void u0(void);
extern void u1(void);
extern void u2(void);
extern void u3(void);
extern void u4(void);
extern void u5(void);
extern void u6(void);
extern void u7(void);
extern void u8(void);
extern void u9(void);
extern void v0(void);
extern void v1(void);
extern void v2(void);
extern void v3(void);
extern void v4(void);
extern void v5(void);
extern void v6(void);
extern void v7(void);
extern void v8(void);
extern void v9(void);
extern void w0(void);
extern void w1(void);
extern void w2(void);
extern void w3(void);
extern void w4(void);
extern void w5(void);
extern void w6(void);
extern void w7(void);
extern void w8(void);
extern void w9(void);
extern void x0(void);
extern void x1(void);
extern void x2(void);
extern void x3(void);
extern void x4(void);
extern void x5(void);
extern void x6(void);
extern void x7(void);
extern void x8(void);
extern void x9(void);
extern void y0(void);
extern void y1(void);
extern void y2(void);
extern void y3(void);
extern void y4(void);
extern void y5(void);
extern void y6(void);
extern void y7(void);
extern void y8(void);
extern void y9(void);
extern void z0(void);
extern void z1(void);
extern void z2(void);
extern void z3(void);
extern void z4(void);
extern void z5(void);
extern void z6(void);
extern void z7(void);
extern void z8(void);
extern void z9(void);

void
callem(void)
{
	a0();
	a1();
	a2();
	a3();
	a4();
	a5();
	a6();
	a7();
	a8();
	a9();
	b0();
	b1();
	b2();
	b3();
	b4();
	b5();
	b6();
	b7();
	b8();
	b9();
	c0();
	c1();
	c2();
	c3();
	c4();
	c5();
	c6();
	c7();
	c8();
	c9();
	d0();
	d1();
	d2();
	d3();
	d4();
	d5();
	d6();
	d7();
	d8();
	d9();
	e0();
	e1();
	e2();
	e3();
	e4();
	e5();
	e6();
	e7();
	e8();
	e9();
	f0();
	f1();
	f2();
	f3();
	f4();
	f5();
	f6();
	f7();
	f8();
	f9();
	g0();
	g1();
	g2();
	g3();
	g4();
	g5();
	g6();
	g7();
	g8();
	g9();
	h0();
	h1();
	h2();
	h3();
	h4();
	h5();
	h6();
	h7();
	h8();
	h9();
	i0();
	i1();
	i2();
	i3();
	i4();
	i5();
	i6();
	i7();
	i8();
	i9();
	j0();
	j1();
	j2();
	j3();
	j4();
	j5();
	j6();
	j7();
	j8();
	j9();
	k0();
	k1();
	k2();
	k3();
	k4();
	k5();
	k6();
	k7();
	k8();
	k9();
	l0();
	l1();
	l2();
	l3();
	l4();
	l5();
	l6();
	l7();
	l8();
	l9();
	m0();
	m1();
	m2();
	m3();
	m4();
	m5();
	m6();
	m7();
	m8();
	m9();
	n0();
	n1();
	n2();
	n3();
	n4();
	n5();
	n6();
	n7();
	n8();
	n9();
	o0();
	o1();
	o2();
	o3();
	o4();
	o5();
	o6();
	o7();
	o8();
	o9();
	p0();
	p1();
	p2();
	p3();
	p4();
	p5();
	p6();
	p7();
	p8();
	p9();
	q0();
	q1();
	q2();
	q3();
	q4();
	q5();
	q6();
	q7();
	q8();
	q9();
	r0();
	r1();
	r2();
	r3();
	r4();
	r5();
	r6();
	r7();
	r8();
	r9();
	s0();
	s1();
	s2();
	s3();
	s4();
	s5();
	s6();
	s7();
	s8();
	s9();
	t0();
	t1();
	t2();
	t3();
	t4();
	t5();
	t6();
	t7();
	t8();
	t9();
	u0();
	u1();
	u2();
	u3();
	u4();
	u5();
	u6();
	u7();
	u8();
	u9();
	v0();
	v1();
	v2();
	v3();
	v4();
	v5();
	v6();
	v7();
	v8();
	v9();
	w0();
	w1();
	w2();
	w3();
	w4();
	w5();
	w6();
	w7();
	w8();
	w9();
	x0();
	x1();
	x2();
	x3();
	x4();
	x5();
	x6();
	x7();
	x8();
	x9();
	y0();
	y1();
	y2();
	y3();
	y4();
	y5();
	y6();
	y7();
	y8();
	y9();
	z0();
	z1();
	z2();
	z3();
	z4();
	z5();
	z6();
	z7();
	z8();
	z9();
}
