/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.148 $"

#include <sys/proc.h>
#include <sys/syscall.h>

/*
 * This table is the switch used to transfer
 * to the appropriate routine for processing a system call.
 * Each row contains the number of arguments expected, a switch
 * telling systrap() in trap.c whether a setjmp() is necessary,
 * and a pointer to the routine.
 *
 * NOTE: be sure to keep other ABI sysent tables up to date!
 */

/*
 * System V.3 entry points.
 */
int	access();
int	alarm();
int	chdir();
int	chmod();
int	chown();
int	chroot();
int	close();
int	creat();
int	dup();
int	exec();
int	exece();
int	fcntl();
int	fork();
int	fstatfs();
int	getdents();
int	getgid();
int	getmsg();
int	getpmsg();
int	getpid();
int	getuid();
int	gtime();
int	ioctl();
int	kill();
int	link();
int	lock();
int	lseek();
int	mkdir();
int	mount();
int	msgsys();
int	nice();
int	nosys();
int	nullsys();
int	open();
int	pause();
int	pipe();
int	poll();
int	profil();
int	ptrace();
int	putmsg();
int	putpmsg();
int	read();
int	readv();
int	rexit();
int	rmdir();
int	sbreak();
int	select();
int	xpg4_select();
int	semsys();
int	setgid();
int	setpgrp();
int	setuid();
int	shmsys();
int	statfs();
int	stime();
int	sync();
int	sysacct();
int	sysfs();
int	sysget();
int	sysmips();
int	sysmp();
int	syssgi();
int	times();
int	uadmin();
int	ulimit();
int	umask();
int	umount();
int	unlink();
int	utime();
int	utssys();
int	write();
int	writev();
int	sigreturn();

/*
 * System V.4 entry points.
 */
int	systeminfo();
int	uname();
int	xstat();
int	lxstat();
int	fxstat();
int	xmknod();
int	statvfs();
int	fstatvfs();
int	lchown();
int	priocntl();
int	nopkg();
int	pread();
int	pwrite();

/*
 * 4.3BSD networking.
 */
int	accept();
int	bind();
int	connect();
int	gethostid();
int	getpeername();
int	getsockname();
int	getsockopt();
int	listen();
int	recv();
int	recvfrom();
int	recvmsg();
int	xpg4_recvmsg();
int	send();
int	sendmsg();
int	sendto();
int	sethostid();
int	setsockopt();
int	shutdown();
int	socket();
int	gethostname();
int	sethostname();
int	getdomainname();
int	setdomainname();

/*
 * 4.3BSD compatibility
 */
int	BSDgetpgrp();
int	BSDsetpgrp();
int	adjtime();
int	fchdir();
int	fchmod();
int	fchown();
int	fsync();
int	ftruncate();
int	getitimer();
int	getpagesize();
int	getrlimit();
int	gettimeofday();
int	quotactl();
int	readlink();
int	rename();
int	setitimer();
int	setregid();
int	xpg4_setregid();
int	setreuid();
int	setrlimit();
int	socketpair();
int	symlink();
int	truncate();
int	vhangup();

/*
 * NFS system calls.
 */
int	async_daemon();
int	exportfs();
int	nfs_getfh();
int	nfs_svc();

/*
 * SGI extensions.
 */

int	sginap();
int	sgikopt();

int	prctl();
int	procblk();
int	sproc();
int	sprocsp();
int	nsproc();
int	rexec();

int	cacheflush();
int	cachectl();

int	mmap();
int	munmap();
int	mprotect();
int	msync();
int	madvise();
int	pagelock();

int	sigpending();
int	sigprocmask();
int	sigsuspend();
int	sigpoll_sys();
int	swapctl();
int	getcontext();
int	setcontext();
int	sigaction();
int	waitsys_wrapper();
int	sigstack();
int	sigaltstack();
int	xpg4_sigaltstack();
int	sigsendsys();
int	sigqueue_sys();

int	lseek64();
int	truncate64();
int	ftruncate64();
int	getrlimit64();
int	setrlimit64();
int	mmap64();

int	dmi();
int     umfscall();

int	attr_get();
int	attr_getf();
int	attr_set();
int	attr_setf();
int	attr_remove();
int	attr_removef();
int	attr_list();
int	attr_listf();
int	attr_multi();
int	attr_multif();

int	statvfs64();
int	fstatvfs64();

int	getmountid();
int	getdents64();
int	ngetdents();
int	ngetdents64();

int     afs_syscall();
int	linkfollow();

/*
 * POSIX 1003.1 syscalls
 */
int	fdatasync();
int	nanosleep();
int	sched_rr_get_interval();
int	sched_getparam();
int	sched_getscheduler();
int	sched_setparam();
int	sched_setscheduler();
int	sched_yield();
int	timer_create();
int	timer_delete();
int	timer_settime();
int	timer_gettime();
int	timer_getoverrun();

/*
 * synchronization module for Posix.1b support
 */
int	usync_cntl();
/*
 * Posix.1b named semaphores
 */
int	psema_cntl();

#ifdef CKPT
int	pidsprocsp();
int	ckpt_restartreturn();
int	nsproctid();
#endif

/*
 * Trusted Irix
 */
int	sgi_sesmgr();

#ifdef CELL_IRIX
int	rexec_complete();
#endif

/*
 * Descriptors that are common across a set of ABIs
 */
static sysargdesc_t __arg_string1[] = { /* calls with arg1 == string */
	{SY_ARG(1)|SY_STRING},
	{0}
};

static sysargdesc_t __arg_string2[] = { /* calls with arg2 == string */
	{SY_ARG(2)|SY_STRING},
	{0}
};

static sysargdesc_t __arg_string12[] = {
	{SY_ARG(1)|SY_STRING},
	{SY_ARG(2)|SY_STRING},
	{0}
};

static sysargdesc_t __arg_wsockadd[] = { /* write arg2 == sockaddr, arg3 == len */
	{SY_ARG(2)|SY_INV|SY_VARG(3)},
	{0}
};

static sysargdesc_t __arg_rsockadd[] = { /* read arg2 == sockaddr, arg3 == len */
	{SY_ARG(2)|SY_OUTVI|SY_VARG(3)},
	{SY_ARG(3)|SY_IN|4},
	{SY_ARG(3)|SY_OUT|4},
	{0}
};

static sysargdesc_t __arg_wvs1[] = {	/* write variable string arg 1 */
	{SY_ARG(1)|SY_STRING|SY_VARG(2)},
	{0}
};

static sysargdesc_t __arg_rvs1[] = {	/* read variable string arg 1 */
	{SY_ARG(1)|SY_OUTSTRING|SY_VARG(2)},
	{0}
};

static sysargdesc_t __arg_write[] = {
	{SY_ARG(2)|SY_INV|SY_VARG(3)},
	{0}
};

static sysargdesc_t __arg_read[] = {
	{SY_ARG(2)|SY_OUTVI|0},
	{0}
};

static sysargdesc_t __arg_sigsuspend[] = {
	{SY_ARG(1)|SY_IN|16},
	{0}
};

static sysargdesc_t __arg_sigsendset[] = {
	{SY_ARG(1)|SY_IN|20},
	{0}
};

static sysargdesc_t __arg_sigprocmask[] = {
	{SY_ARG(2)|SY_IN|16},
	{SY_ARG(3)|SY_OUT|16},
	{0}
};

static sysargdesc_t __arg_sigpending[] = {
	{SY_ARG(1)|SY_OUT|16},
	{0}
};

static sysargdesc_t __arg_socketpair[] = {
	{SY_ARG(3)|SY_OUT|8},
	{0}
};

static sysargdesc_t __arg_readlink[] = {
	{SY_ARG(1)|SY_STRING},
	{SY_ARG(2)|SY_OUTSTRING|SY_VARG(3)},
	{0}
};

static sysargdesc_t __arg_recv[] = {
	{SY_ARG(2)|SY_OUTVI|0},
	{0}
};

static sysargdesc_t __arg_send[] = {
	{SY_ARG(2)|SY_INV|SY_VARG(3)},
	{0}
};

static sysargdesc_t __arg_recvfrom[] = {
	{SY_ARG(2)|SY_OUTVI|0},
	{SY_ARG(5)|SY_OUTVI|SY_VARG(6)},
	{SY_ARG(6)|SY_IN|4},
	{SY_ARG(6)|SY_OUT|4},
	{0}
};

static sysargdesc_t __arg_sendto[] = {
	{SY_ARG(2)|SY_INV|SY_VARG(3)},
	{SY_ARG(5)|SY_INV|SY_VARG(6)},
	{0}
};

static sysargdesc_t __arg_rwv[] = {
	{SY_ARG(2)|SY_INSPECIAL|SY_SPIDX(3)},
	{0}
};

static sysargdesc_t __arg_umfscall[] = {
	{SY_ARG(4)|SY_INV|SY_VARG(3)},
	{SY_ARG(6)|SY_OUTVI|SY_VARG(5)},
	{0}
};

#define IRIX5_IN_TIMEVAL	(SY_IN|8) /* timeval and timespec */
#define IRIX5_OUT_TIMEVAL	(SY_OUT|8)
#define IRIX5_OUT_SIGINFO	(SY_OUT|28)
#define IRIX5_IN_SIGACTION	(SY_IN|32)
#define IRIX5_OUT_SIGACTION	(SY_OUT|32)

#define IRIX5_64_IN_TIMEVAL	(SY_IN|16) /* timeval and timespec */
#define IRIX5_64_OUT_TIMEVAL	(SY_OUT|16)
#define IRIX5_64_OUT_SIGINFO	(SY_OUT|40)
#define IRIX5_64_IN_SIGACTION	(SY_IN|40)
#define IRIX5_64_OUT_SIGACTION	(SY_OUT|40)

static sysargdesc_t irix5_arg_select[] = {
	{SY_ARG(2)|SY_INSPECIAL|SY_SPIDX(0)},
	{SY_ARG(3)|SY_INSPECIAL|SY_SPIDX(0)},
	{SY_ARG(4)|SY_INSPECIAL|SY_SPIDX(0)},
	{SY_ARG(2)|SY_OUTSPECIAL|SY_SPIDX(0)},
	{SY_ARG(3)|SY_OUTSPECIAL|SY_SPIDX(0)},
	{SY_ARG(4)|SY_OUTSPECIAL|SY_SPIDX(0)},
	{SY_ARG(5)|IRIX5_IN_TIMEVAL},
	{0}
};

static sysargdesc_t arg_fcntl[] = {
	{SY_ARG(3)|SY_INSPECIAL|SY_SPIDX(1)},
	{SY_ARG(3)|SY_OUTSPECIAL|SY_SPIDX(1)},
	{0}
};

static sysargdesc_t arg_ioctl[] = {
	{SY_ARG(3)|SY_INSPECIAL|SY_SPIDX(4)},
	{SY_ARG(3)|SY_OUTSPECIAL|SY_SPIDX(4)},
	{0}
};

static sysargdesc_t irix5_arg_nanosleep[] = {
	{SY_ARG(1)|IRIX5_IN_TIMEVAL},
	{SY_ARG(2)|IRIX5_OUT_TIMEVAL},
	{0}
};

static sysargdesc_t irix5_arg_sched_rr_get_interval[] = {
	{SY_ARG(1)|SY_IN},
	{SY_ARG(2)|IRIX5_OUT_TIMEVAL},
	{0}
};

static sysargdesc_t irix5_arg_sigaction[] = {
	{SY_ARG(2)|IRIX5_IN_SIGACTION},
	{SY_ARG(3)|IRIX5_OUT_SIGACTION},
	{0}
};

static sysargdesc_t irix5_arg_waitsys[] = {
	{SY_ARG(3)|IRIX5_OUT_SIGINFO},
	{0}
};

static sysargdesc_t irix5_arg_sigpoll[] = {
	{SY_ARG(1)|SY_IN|16},
	{SY_ARG(2)|IRIX5_OUT_SIGINFO},
	{SY_ARG(3)|IRIX5_IN_TIMEVAL},
	{0}
};

static sysargdesc_t irix5_arg_gettimeofday[] = {
	{SY_ARG(1)|IRIX5_OUT_TIMEVAL},
	{0}
};

static sysargdesc_t arg_poll[] = {
	{SY_ARG(1)|SY_INSPECIAL|SY_SPIDX(2)},
	{SY_ARG(1)|SY_OUTSPECIAL|SY_SPIDX(2)},
	{0}
};

static sysargdesc_t __arg_attr_get[] = {
	{SY_ARG(1)|SY_STRING},
	{SY_ARG(2)|SY_STRING},
	{SY_ARG(3)|SY_OUTVI|SY_VARG(4)},
	{SY_ARG(4)|SY_OUT|4},
	{0}
};
static sysargdesc_t __arg_attr_getf[] = {
	{SY_ARG(2)|SY_STRING},
	{SY_ARG(3)|SY_OUTVI|SY_VARG(4)},
	{SY_ARG(4)|SY_OUT|4},
	{0}
};
static sysargdesc_t __arg_attr_set[] = {
	{SY_ARG(1)|SY_STRING},
	{SY_ARG(2)|SY_STRING},
	{SY_ARG(3)|SY_INV|SY_VARG(4)},
	{SY_ARG(4)|SY_IN|4},
	{0}
};
static sysargdesc_t __arg_attr_setf[] = {
	{SY_ARG(2)|SY_STRING},
	{SY_ARG(3)|SY_INV|SY_VARG(4)},
	{SY_ARG(4)|SY_IN|4},
	{0}
};

#if _MIPS_SIM == _ABI64
static sysargdesc_t irix5_64_arg_select[] = {
	{SY_ARG(2)|SY_INSPECIAL|SY_SPIDX(0)},
	{SY_ARG(3)|SY_INSPECIAL|SY_SPIDX(0)},
	{SY_ARG(4)|SY_INSPECIAL|SY_SPIDX(0)},
	{SY_ARG(2)|SY_OUTSPECIAL|SY_SPIDX(0)},
	{SY_ARG(3)|SY_OUTSPECIAL|SY_SPIDX(0)},
	{SY_ARG(4)|SY_OUTSPECIAL|SY_SPIDX(0)},
	{SY_ARG(5)|IRIX5_64_IN_TIMEVAL},
	{0}
};

static sysargdesc_t irix5_64_arg_nanosleep[] = {
	{SY_ARG(1)|IRIX5_64_IN_TIMEVAL},
	{SY_ARG(2)|IRIX5_64_OUT_TIMEVAL},
	{0}
};

static sysargdesc_t irix5_64_arg_sched_rr_get_interval[] = {
	{SY_ARG(1)|SY_IN},
	{SY_ARG(2)|IRIX5_64_OUT_TIMEVAL},
	{0}
};

static sysargdesc_t irix5_64_arg_sigaction[] = {
	{SY_ARG(2)|IRIX5_64_IN_SIGACTION},
	{SY_ARG(3)|IRIX5_64_OUT_SIGACTION},
	{0}
};

static sysargdesc_t irix5_64_arg_waitsys[] = {
	{SY_ARG(3)|IRIX5_64_OUT_SIGINFO},
	{0}
};

static sysargdesc_t irix5_64_arg_sigpoll[] = {
	{SY_ARG(1)|SY_IN|16},
	{SY_ARG(2)|IRIX5_64_OUT_SIGINFO},
	{SY_ARG(3)|IRIX5_64_IN_TIMEVAL},
	{0}
};

static sysargdesc_t irix5_64_arg_gettimeofday[] = {
	{SY_ARG(1)|IRIX5_64_OUT_TIMEVAL},
	{0}
};
#endif	/* _ABI64 */


/*
 * Irix 5 ABI
 */

struct sysent sysent[] =
{
	0, 0, nosys,			/*  0 = indir */
		{0},
	1, 0, rexit,			/*  1 = exit */
		{0},
	0, SY_SPAWNER, fork,		/*  2 = fork */
		{0},
	3, 0, read,			/*  3 = read */
		__arg_read,
	3, 0, write,			/*  4 = write */
		__arg_write,
	3, SY_NOXFSZ, open,		/*  5 = open */
		__arg_string1,
	1, 0, close,			/*  6 = close */
		{0},
	0, 0, nosys,			/*  7 = IRIX4 wait */	
		{0},
	2, SY_NOXFSZ, creat,		/*  8 = creat */
		__arg_string1,
	2, 0, link,			/*  9 = link */
		__arg_string12,
	1, 0, unlink,			/* 10 = unlink */
		__arg_string1,
	2, 0, exec,			/* 11 = exec */
		__arg_string1,
	1, 0, chdir,			/* 12 = chdir */
		__arg_string1,
	0, 0, gtime,			/* 13 = time */
		{0},
	0, 0, nosys,			/* 14 = IRIX4 mknod */
		{0},
	2, 0, chmod,			/* 15 = chmod */
		__arg_string1,
	3, 0, chown,			/* 16 = chown; now 3 args */
		__arg_string1,
	1, 0, sbreak,			/* 17 = break */
		{0},
	0, 0, nosys,			/* 18 = IRIX4 stat */
		{0},
	3, SY_64BIT_ARG, lseek,		/* 19 = lseek */
		{0},
	0, 0, getpid,			/* 20 = getpid */
		{0},
	6, 0, mount,			/* 21 = mount */
		__arg_string12,
	1, 0, umount,			/* 22 = umount */
		__arg_string1,
	1, 0, setuid,			/* 23 = setuid */
		{0},
	0, 0, getuid,			/* 24 = getuid */
		{0},
	1, 0, stime,			/* 25 = stime */
		{0},
	4, 0, ptrace,			/* 26 = ptrace */
		{0},
	1, 0, alarm,			/* 27 = alarm */
		{0},
	0, 0, nosys,			/* 28 = IRIX4 fstat */
		{0},
	0, SY_NORESTART, pause,		/* 29 = pause */
		{0},
	2, 0, utime,			/* 30 = utime */
		__arg_string1,
	0, 0, nosys,			/* 31 = */
		{0},
	0, 0, nosys,			/* 32 = */
		{0},
	2, 0, access,			/* 33 = access */
		__arg_string1,
	1, 0, nice,			/* 34 = nice */
		{0},
	4, 0, statfs,			/* 35 = statfs */
		__arg_string1,
	0, 0, sync,			/* 36 = sync */
		{0},
	2, 0, kill,			/* 37 = kill */
		{0},
	4, 0, fstatfs, 			/* 38 = fstatfs */
		{0},
	1, 0, setpgrp,			/* 39 = setpgrp */
		{0},
	6, SY_64BIT_ARG, syssgi,	/* 40 = SGI-specific system call */
		{0},
	1, 0, dup,			/* 41 = dup */
		{0},
	0, 0, pipe,			/* 42 = pipe */
		{0},
	1, 0, times,			/* 43 = times */
		{0},
	4, 0, profil,			/* 44 = profil */
		{0},
	1, 0, lock,			/* 45 = proc lock */
		{0},
	1, 0, setgid,			/* 46 = setgid */
		{0},
	0, 0, getgid,			/* 47 = getgid */
		{0},
	0, 0, nosys,			/* 48 = IRIX4 ssig */ 
		{0},
	6, 0, msgsys,			/* 49 = IPC message */
		{0},
	4, 0, sysmips,			/* 50 = mips-specific system call */
		{0},
	1, 0, sysacct,			/* 51 = turn acct off/on */
		{0},
	5, 0, shmsys,			/* 52 = shared memory */
		{0},
	5, SY_NOXFSZ|SY_64BIT_ARG, semsys,/* 53 = IPC semaphores */
		{0},
	3, 0, ioctl,			/* 54 = ioctl */
		arg_ioctl,
	3, 0, uadmin,			/* 55 = uadmin */
		{0},
	5, 0, sysmp,			/* 56 = mp - specific calls */
		{0},
	3, 0, utssys,			/* 57 = utssys */
		{0},
	0, 0, nosys,			/* 58 */
		{0},
	3, 0, exece,			/* 59 = exece */
		__arg_string1,
	1, 0, umask,			/* 60 = umask */
		{0},
	1, 0, chroot,			/* 61 = chroot */
		__arg_string1,
	3, 0, fcntl,			/* 62 = fcntl */
		arg_fcntl,
	2, 0, ulimit,			/* 63 = ulimit */
		{0},

	0, 0, nopkg,			/* 64 AFS */
		{0},
	0, 0, nopkg,			/* 65 AFS */
		{0},
	0, 0, nopkg,			/* 66 AFS */
		{0},
	0, 0, nopkg,			/* 67 AFS */
		{0},
	0, 0, nopkg,			/* 68 AFS */
		{0},
	0, 0, nopkg,			/* 69 AFS */
		{0},
	0, 0, nopkg,			/* 70 AFS */
		{0},
	0, 0, nopkg,			/* 71 AFS */
		{0},
	0, 0, nopkg,			/* 72 AFS */
		{0},
	0, 0, nopkg,			/* 73 AFS */
		{0},

	0, 0, nosys,			/* 74 */
		{0},
	2, 0, getrlimit64,		/* 75 = getrlimit64 */
		{0},
	2, 0, setrlimit64,		/* 76 = setrlimit64 */
		{0},
	2, 0, nanosleep,		/* 77 = nanosleep */
		irix5_arg_nanosleep,
	5, 0, lseek64,			/* 78 = lseek64 */
		{0},

	1, 0, rmdir,			/* 79 = rmdir */
		__arg_string1,
	2, 0, mkdir,			/* 80 = mkdir */
		__arg_string1,
	3, 0, getdents,			/* 81 = getdents */
		{0},
	1, 0, sginap,			/* 82 = sginap */
		{0},
	3, 0, sgikopt,			/* 83 = sgikopt */
		{0},
	3, 0, sysfs,			/* 84 = sysfs */
		{0},
	4, 0, getmsg,			/* 85 = getmsg */
		{0},
	4, 0, putmsg,			/* 86 = putmsg */
		{0},
	3, 0, poll,			/* 87 = poll */
		arg_poll,

	3, SY_FULLRESTORE, sigreturn,	/* 88 = sig return */
		{0},

/* 4.3BSD-compatible system calls */
	3, 0,	accept,			/* 89 =  accept */
		__arg_rsockadd,
	3, 0,	bind,			/* 90 =  bind */
		__arg_wsockadd,
	3, 0,	connect,		/* 91 =  connect */
		__arg_wsockadd,
	0, 0,	gethostid,		/* 92 =  gethostid */
		{0},
	3, 0,	getpeername,		/* 93 =  getpeername */
		__arg_rsockadd,
	3, 0,	getsockname,		/* 94 =  getsockname */
		__arg_rsockadd,
	5, 0,	getsockopt,		/* 95 =  getsockopt */
		{0},
	2, 0,	listen,			/* 96 =  listen */
		{0},
	4, 0,	recv,			/* 97 =  recv */
		__arg_recv,
	6, 0,	recvfrom,		/* 98 =  recvfrom */
		__arg_recvfrom,
	3, 0,	recvmsg,		/* 99 =  recvmsg */
		{0},
	5, 0,	select,			/* 100 = select */
		irix5_arg_select,
	4, 0,	send,			/* 101 = send */
		__arg_send,
	3, 0,	sendmsg,		/* 102 = sendmsg */
		{0},
	6, 0,	sendto,			/* 103 = sendto */
		__arg_sendto,
	1, 0,	sethostid,		/* 104 = sethostid */
		{0},
	5, 0,	setsockopt,		/* 105 = setsockopt */
		{0},
	2, 0,	shutdown,		/* 106 = shutdown */
		{0},
	3, 0,	socket,			/* 107 = socket */
		{0},
	2, 0,		gethostname,	/* 108 = gethostname */
		__arg_rvs1,
	2, 0,		sethostname,	/* 109 = sethostname */
		__arg_wvs1,
	2, 0,		getdomainname,	/* 110 = getdomainname */
		__arg_rvs1,
	2, 0,		setdomainname,	/* 111 = setdomainname */
		__arg_wvs1,

	2, SY_64BIT_ARG, truncate,	/* 112 = truncate */
		__arg_string1,
	2, SY_64BIT_ARG, ftruncate,	/* 113 = ftruncate */
		{0},
	2, 0, rename,			/* 114 = rename */
		__arg_string12,
	2, 0, symlink,			/* 115 = create symbolic link */
		__arg_string12,
	3, 0, readlink,			/* 116 = read symbolic link */
		__arg_readlink,
	0, 0, nosys,			/* 117 = IRIX4 lstat */
		{0},

/* Network File System system calls */
	0, 0, nosys,			/* 118 = */
		{0},
	4, 0, nfs_svc,			/* 119 = nfs server */
		{0},
	2, 0, nfs_getfh,		/* 120 = get file handle */
		{0},
	0, 0, async_daemon,		/* 121 = nfs bio daemon */
		{0},
	3, 0, exportfs,			/* 122 = export served filesystem */
		{0},

/* more 4.3BSD system calls */
	2, 0, setregid,			/* 123 = set real,eff. gid */
		{0},
	2, 0, setreuid,			/* 123 = set real,eff. uid */
		{0},
	2, 0, getitimer,		/* 125 = getitimer */
		{0},
	3, 0, setitimer,		/* 126 = setitimer */
		{0},
	2, 0, adjtime,			/* 127 = adjtime */
		{0},
	1, 0, gettimeofday,		/* 128 = gettimeofday */
		irix5_arg_gettimeofday,

/* shared process system calls */
	3, SY_SPAWNER, sproc,		/* 129 = sproc */
		{0},
	5, 0, prctl,			/* 130 = prctl */
		{0},
	3, 0, procblk,			/* 131 = procblk */
		{0},
	5, SY_SPAWNER, sprocsp,		/* 132 = sprocsp */
		{0},
	0, 0, nosys,			/* 133 = (old) sgigsc */
		{0},

/* memory-mapped file calls */
	6, SY_64BIT_ARG, mmap,		/* 134 = mmap */
		{0},
	2, 0, munmap,			/* 135 = munmap */
		{0},
	3, 0, mprotect,			/* 136 = mprotect */
		{0},
	3, 0, msync,			/* 137 = msync */
		{0},
	3, 0, madvise,			/* 138 = madvise */
		{0},
	3, 0, pagelock,			/* 139 = mpin, munpin */
		{0},
	0, 0, getpagesize,		/* 140 = getpagesize */
		{0},

	4, 0, quotactl,			/* 141 = quotactl */
		{0},
	0, 0, nosys,			/* 142 = */
		{0},
	1, 0, BSDgetpgrp,		/* 143 = BSD style getpgrp */
		{0},
	2, 0, BSDsetpgrp,		/* 144 = BSD style setpgrp */
		{0},
	0, 0, vhangup,			/* 145 = BSD vhangup	*/
		{0},
	1, 0, fsync,			/* 146 = BSD fsync	*/
		{0},
	1, 0, fchdir,			/* 147 = BSD fchdir	*/
		{0},
	2, 0, getrlimit,		/* 148 = getrlimit */
		{0},
	2, 0, setrlimit,		/* 149 = setrlimit */
		{0},
	3, 0, cacheflush,		/* 150 = cacheflush */
		{0},
	3, 0, cachectl,			/* 151 = cachectl */
		{0},
	3, 0, fchown,			/* 152 = fchown */
		{0},
	2, 0, fchmod,			/* 153 = fchmod */
		{0},
	0, 0, nosys,			/* 154 = IRIX4 wait3 */ 
		{0},

/* more 4.3BSD-compatible socket/protocol system calls */
	4, 0, socketpair,		/* 155 =  socketpair */
		__arg_socketpair,

/* new SVR4 system calls */
	3, 0, systeminfo,               /* 156 = systeminfo */
		{0},
	1, 0, uname,               	/* 157 = uname */
		{0},
        3, 0, xstat,			/* 158 = xstat */
		__arg_string2,
        3, 0, lxstat,			/* 159 = lxstat */
		__arg_string2,
        3, 0, fxstat,			/* 160 = fxstat */
		{0},
        4, 0, xmknod,			/* 161 = xmknod */
		__arg_string2,
/* Posix.1 signal calls */
	4, 0, sigaction,		/* 162 = sigaction */
		irix5_arg_sigaction,
	1, 0, sigpending,		/* 163 = sigpending */
		__arg_sigpending,
	3, 0, sigprocmask,		/* 164 = sigprocmask */
		__arg_sigprocmask,
	1, SY_NORESTART, sigsuspend,	/* 165 = sigsuspend */
		__arg_sigsuspend,
	3, 0, sigpoll_sys,		/* 166 = sigpoll */
		irix5_arg_sigpoll,
	2, 0, swapctl,			/* 167 = swapctl */
		{0},
	1, 0, getcontext,		/* 168 = getcontext */
		{0},
	1, SY_FULLRESTORE, setcontext,	/* 169 = setcontext */
		{0},
	5, 0, waitsys_wrapper,		/* 170 = waitsys */
		irix5_arg_waitsys,
	2, 0, sigstack,			/* 171 = sigstack */
		{0},
	2, 0, sigaltstack,		/* 172 = sigaltstack */
		{0},
	2, 0, sigsendsys,		/* 173 = sigsendset */
		__arg_sigsendset,
	2, 0, statvfs,			/* 174 = statvfs */
		__arg_string1,
	2, 0, fstatvfs,			/* 175 = fstatvfs */
		{0},
	5, 0, getpmsg,			/* 176 = getpmsg */
		{0},
	5, 0, putpmsg,			/* 177 = putpmsg */
		{0},
	3, 0, lchown,			/* 178 = lchown */
		__arg_string1,
	0, 0, nopkg,			/* 179 = priocntl */ 
		{0},
	4, 0, sigqueue_sys,		/* 180 = posix.4 D12 sigqueue */
		{0},
	3, 0, readv,        		/* 181 = readv */
		__arg_rwv,
	3, 0, writev,        		/* 182 = writev */
		__arg_rwv,
	4, 0, truncate64,		/* 183 = truncate64 */
		__arg_string1,
	4, 0, ftruncate64,		/* 184 = ftruncate64 */
		{0},
	8, 0, mmap64,			/* 185 = mmap64 */
		{0},
	8, 0, dmi,			/* 186 = dmi */
		{0},
	6, SY_64BIT_ARG, pread,		/* 187 = pread */
		__arg_read,
	6, SY_64BIT_ARG, pwrite,	/* 188 = pwrite */
		__arg_write,
	1, 0, fdatasync,		/* 189 = POSIX fdatasync */
		{0},	
	6, 0, nosys,			/* 190 = was sgifastpath */
		{0},

	5, 0, attr_get,			/* 191 = attr_get */
		__arg_attr_get,
	5, 0, attr_getf,		/* 192 = attr_getf */
		__arg_attr_getf,
	5, 0, attr_set,			/* 193 = attr_set */
		__arg_attr_set,
	5, 0, attr_setf,		/* 194 = attr_setf */
		__arg_attr_setf,
	3, 0, attr_remove,		/* 195 = attr_remove */
		__arg_string12,
	3, 0, attr_removef,		/* 196 = attr_removef */
		__arg_string2,
	5, 0, attr_list,		/* 197 = attr_list */
		{0},
	5, 0, attr_listf,		/* 198 = attr_listf */
		{0},
	4, 0, attr_multi,		/* 199 = attr_multi */
		__arg_string1,
	4, 0, attr_multif,		/* 200 = attr_multif */
		{0},

	2, 0, statvfs64,		/* 201 = statvfs64 */
		__arg_string1,
	2, 0, fstatvfs64,		/* 202 = fstatvfs64 */
		{0},

	2, 0, getmountid,		/* 203 = getmountid */
		__arg_string1,
	
/* new sproc */	
	5, SY_SPAWNER, nsproc,		/* 204 = nsproc */
		{0},

	3, 0, getdents64,		/* 205 = getdents64 */
		{0},
	6, 0, afs_syscall,		/* 206 = reserved for DFS */
		{0},
	4, 0, ngetdents,		/* 207 = ngetdents */
		{0},
	4, 0, ngetdents64,		/* 208 = ngetdents64 */
		{0},
	8, SY_64BIT_ARG, sgi_sesmgr,	/* 209 = Trix Session Manager */
		{0},
#ifdef CKPT
	6, SY_SPAWNER, pidsprocsp,	/* 210 = pidsprocsp */
		{0},
#else /* CKPT */
	0, 0, nosys,			/* 210 = */
		{0},
#endif /* CKPT */
	4, 0, rexec,			/* 211 = rexec */
		__arg_string2,
	3, 0, timer_create,		/* 212 = timer_create */
		{0},
	1, 0, timer_delete,		/* 213 = timer_delete */
		{0},
	4, 0, timer_settime,		/* 214 = timer_settime */
		{0},
	2, 0, timer_gettime,		/* 215 = timer_gettime */
		{0},
	1, 0, timer_getoverrun,		/* 216 = timer_getoverrun */
		{0},
	2, 0, sched_rr_get_interval,	/* 217 = sched_rr_get_interval */
		irix5_arg_sched_rr_get_interval,
	0, 0, sched_yield,		/* 218 = sched_yield */
		{0},
	1, 0, sched_getscheduler,	/* 219 = sched_getscheduler */
		{0},
	3, 0, sched_setscheduler,	/* 220 = sched_setscheduler */
		{0},
	2, 0, sched_getparam,		/* 221 = sched_getparam */
		{0},
	2, 0, sched_setparam,		/* 222 = sched_setparam */
		{0},
	2, 0, usync_cntl,		/* 223 = usync_cntl */
		{0},
	5, 0, psema_cntl,		/* 224 = psema_cntl */
		__arg_string2,
#ifdef CKPT
	3, SY_FULLRESTORE, ckpt_restartreturn,
		{0},			/* 225 = restart return */
#else /* CKPT */
	0, 0, nosys,			/* 225 = RESERVED for CKPT */
		{0},
#endif /* CKPT */
	5, 0, sysget,			/* 226 = sysget */
		{0},
	3, 0,	xpg4_recvmsg,		/* 227 =  xpg4_recvmsg */
		{0},

	5, 0,	umfscall,		/* 228 =  umfscall */
		__arg_umfscall,
#ifdef CKPT
	5, 0,	nsproctid,		/* 229 = nsproctid */
		{0},
#else /* CKPT */
	0, 0, nosys,			/* 229 = RESERVED for CKPT */
		{0},
#endif /* CKPT */

#ifdef CELL_IRIX
	2, 0, rexec_complete,		/* 230 = rexec_complete */
		{0},
#else /* CELL_IRIX */
	0, 0, nosys,			/* 230 = RESERVED for CELL */
		{0},
#endif /* CELL_IRIX */
	2, 0, xpg4_sigaltstack,		/* 231 = xpg4_sigaltstack */
		{0},
	5, 0,	xpg4_select,		/* 232 = xpg4_select */
		irix5_arg_select,
	2, 0, xpg4_setregid,		/* 233 = xpg4_setregid */
		{0},
	2, 0, linkfollow,		/* 234 = linkfollow */
		__arg_string12,
};

#if _MIPS_SIM == _ABI64
struct sysent irix5_64_sysent[] =
{
	0, 0, nosys,			/*  0 = indir */
		{0},
	1, 0, rexit,			/*  1 = exit */
		{0},
	0, SY_SPAWNER, fork,		/*  2 = fork */
		{0},
	3, 0, read,			/*  3 = read */
		__arg_read,
	3, 0, write,			/*  4 = write */
		__arg_write,
	3, SY_NOXFSZ, open,		/*  5 = open */
		__arg_string1,
	1, 0, close,			/*  6 = close */
		{0},
	0, 0, nosys,			/*  7 = IRIX4 wait */	
		{0},
	2, SY_NOXFSZ, creat,		/*  8 = creat */
		__arg_string1,
	2, 0, link,			/*  9 = link */
		__arg_string12,
	1, 0, unlink,			/* 10 = unlink */
		__arg_string1,
	2, 0, exec,			/* 11 = exec */
		__arg_string1,
	1, 0, chdir,			/* 12 = chdir */
		__arg_string1,
	0, 0, gtime,			/* 13 = time */
		{0},
	0, 0, nosys,			/* 14 = IRIX4 mknod */
		{0},
	2, 0, chmod,			/* 15 = chmod */
		__arg_string1,
	3, 0, chown,			/* 16 = chown; now 3 args */
		__arg_string1,
	1, 0, sbreak,			/* 17 = break */
		{0},
	0, 0, nosys,			/* 18 = IRIX4 stat */
		{0},
	3, 0, lseek,			/* 19 = lseek */
		{0},
	0, 0, getpid,			/* 20 = getpid */
		{0},
	6, 0, mount,			/* 21 = mount */
		__arg_string12,
	1, 0, umount,			/* 22 = umount */
		__arg_string1,
	1, 0, setuid,			/* 23 = setuid */
		{0},
	0, 0, getuid,			/* 24 = getuid */
		{0},
	1, 0, stime,			/* 25 = stime */
		{0},
	4, 0, ptrace,			/* 26 = ptrace */
		{0},
	1, 0, alarm,			/* 27 = alarm */
		{0},
	0, 0, nosys,			/* 28 = IRIX4 fstat */
		{0},
	0, SY_NORESTART, pause,		/* 29 = pause */
		{0},
	2, 0, utime,			/* 30 = utime */
		__arg_string1,
	0, 0, nosys,			/* 31 = */
		{0},
	0, 0, nosys,			/* 32 = */
		{0},
	2, 0, access,			/* 33 = access */
		__arg_string1,
	1, 0, nice,			/* 34 = nice */
		{0},
	4, 0, statfs,			/* 35 = statfs */
		__arg_string1,
	0, 0, sync,			/* 36 = sync */
		{0},
	2, 0, kill,			/* 37 = kill */
		{0},
	4, 0, fstatfs, 			/* 38 = fstatfs */
		{0},
	1, 0, setpgrp,			/* 39 = setpgrp */
		{0},
	6, 0, syssgi,			/* 40 = SGI-specific system call */
		{0},
	1, 0, dup,			/* 41 = dup */
		{0},
	0, 0, pipe,			/* 42 = pipe */
		{0},
	1, 0, times,			/* 43 = times */
		{0},
	4, 0, profil,			/* 44 = profil */
		{0},
	1, 0, lock,			/* 45 = proc lock */
		{0},
	1, 0, setgid,			/* 46 = setgid */
		{0},
	0, 0, getgid,			/* 47 = getgid */
		{0},
	0, 0, nosys,			/* 48 = IRIX4 ssig */ 
		{0},
	6, 0, msgsys,			/* 49 = IPC message */
		{0},
	4, 0, sysmips,			/* 50 = mips-specific system call */
		{0},
	1, 0, sysacct,			/* 51 = turn acct off/on */
		{0},
	5, 0, shmsys,			/* 52 = shared memory */
		{0},
	5, SY_NOXFSZ, semsys,		/* 53 = IPC semaphores */
		{0},
	3, 0, ioctl,			/* 54 = ioctl */
		arg_ioctl,
	3, 0, uadmin,			/* 55 = uadmin */
		{0},
	5, 0, sysmp,			/* 56 = mp - specific calls */
		{0},
	3, 0, utssys,			/* 57 = utssys */
		{0},
	0, 0, nosys,			/* 58 */
		{0},
	3, 0, exece,			/* 59 = exece */
		__arg_string1,
	1, 0, umask,			/* 60 = umask */
		{0},
	1, 0, chroot,			/* 61 = chroot */
		__arg_string1,
	3, 0, fcntl,			/* 62 = fcntl */
		arg_fcntl,
	2, 0, ulimit,			/* 63 = ulimit */
		{0},

	0, 0, nopkg,			/* 64 AFS */
		{0},
	0, 0, nopkg,			/* 65 AFS */
		{0},
	0, 0, nopkg,			/* 66 AFS */
		{0},
	0, 0, nopkg,			/* 67 AFS */
		{0},
	0, 0, nopkg,			/* 68 AFS */
		{0},
	0, 0, nopkg,			/* 69 AFS */
		{0},
	0, 0, nopkg,			/* 70 AFS */
		{0},
	0, 0, nopkg,			/* 71 AFS */
		{0},
	0, 0, nopkg,			/* 72 AFS */
		{0},
	0, 0, nopkg,			/* 73 AFS */
		{0},

	0, 0, nosys,			/* 74 */
		{0},
	0, 0, nosys,			/* 75 = getrlimit64 */
		{0},
	0, 0, nosys,			/* 76 = setrlimit64 */
		{0},
	2, 0, nanosleep,		/* 77 = nanosleep */
		irix5_64_arg_nanosleep,
	0, 0, nosys,			/* 78 = lseek64 */
		{0},

	1, 0, rmdir,			/* 79 = rmdir */
		__arg_string1,
	2, 0, mkdir,			/* 80 = mkdir */
		__arg_string1,
	3, 0, getdents,			/* 81 = getdents */
		{0},
	1, 0, sginap,			/* 82 = sginap */
		{0},
	3, 0, sgikopt,			/* 83 = sgikopt */
		{0},
	3, 0, sysfs,			/* 84 = sysfs */
		{0},
	4, 0, getmsg,			/* 85 = getmsg */
		{0},
	4, 0, putmsg,			/* 86 = putmsg */
		{0},
	3, 0, poll,			/* 87 = poll */
		arg_poll,

	3, SY_FULLRESTORE, sigreturn,	/* 88 = sig return */
		{0},

/* 4.3BSD-compatible system calls */
	3, 0, accept,			/* 89 =  accept */
		__arg_rsockadd,
	3, 0, bind,			/* 90 =  bind */
		__arg_wsockadd,
	3, 0, connect,			/* 91 =  connect */
		__arg_wsockadd,
	0, 0, gethostid,		/* 92 =  gethostid */
		{0},
	3, 0, getpeername,		/* 93 =  getpeername */
		__arg_rsockadd,
	3, 0, getsockname,		/* 94 =  getsockname */
		__arg_rsockadd,
	5, 0, getsockopt,		/* 95 =  getsockopt */
		{0},
	2, 0, listen,			/* 96 =  listen */
		{0},
	4, 0, recv,			/* 97 =  recv */
		__arg_recv,
	6, 0, recvfrom,			/* 98 =  recvfrom */
		__arg_recvfrom,
	3, 0, recvmsg,			/* 99 =  recvmsg */
		{0},
	5, 0, select,			/* 100 = select */
		irix5_64_arg_select,
	4, 0, send,			/* 101 = send */
		__arg_send,
	3, 0, sendmsg,			/* 102 = sendmsg */
		{0},
	6, 0, sendto,			/* 103 = sendto */
		__arg_sendto,
	1, 0, sethostid, 		/* 104 = sethostid */
		{0},
	5, 0, setsockopt,		/* 105 = setsockopt */
		{0},
	2, 0, shutdown,			/* 106 = shutdown */
		{0},
	3, 0, socket,			/* 107 = socket */
		{0},
	2, 0, gethostname,		/* 108 = gethostname */
		__arg_rvs1,
	2, 0, sethostname,		/* 109 = sethostname */
		__arg_wvs1,
	2, 0, getdomainname,		/* 110 = getdomainname */
		__arg_rvs1,
	2, 0, setdomainname,		/* 111 = setdomainname */
		__arg_wvs1,

	2, 0, truncate,			/* 112 = truncate */
		__arg_string1,
	2, 0, ftruncate,		/* 113 = ftruncate */
		{0},
	2, 0, rename,			/* 114 = rename */
		__arg_string12,
	2, 0, symlink,			/* 115 = create symbolic link */
		__arg_string12,
	3, 0, readlink,			/* 116 = read symbolic link */
		__arg_readlink,
	0, 0, nosys,			/* 117 = IRIX4 lstat */
		{0},

/* Network File System system calls */
	0, 0, nosys,			/* 118 = */
		{0},
	1, 0, nosys,			/* 119 = nfs server */
		{0},
	2, 0, nfs_getfh,		/* 120 = get file handle */
		{0},
	0, 0, nosys,			/* 121 = nfs bio daemon */
		{0},
	3, 0, exportfs,			/* 122 = export served filesystem */
		{0},

/* more 4.3BSD system calls */
	2, 0, setregid,			/* 123 = set real,eff. gid */
		{0},
	2, 0, setreuid,			/* 123 = set real,eff. uid */
		{0},
	2, 0, getitimer,		/* 125 = getitimer */
		{0},
	3, 0, setitimer,		/* 126 = setitimer */
		{0},
	2, 0, adjtime,			/* 127 = adjtime */
		{0},
	1, 0, gettimeofday,		/* 128 = gettimeofday */
		irix5_64_arg_gettimeofday,

/* shared process system calls */
	3, SY_SPAWNER, sproc,		/* 129 = sproc */
		{0},
	5, 0, prctl,			/* 130 = prctl */
		{0},
	3, 0, procblk,			/* 131 = procblk */
		{0},
	5, SY_SPAWNER, sprocsp,		/* 132 = sprocsp */
		{0},
	0, 0, nosys,			/* 133 = (old) sgigsc */
		{0},

/* memory-mapped file calls */
	6, 0, mmap,			/* 134 = mmap */
		{0},
	2, 0, munmap,			/* 135 = munmap */
		{0},
	3, 0, mprotect,			/* 136 = mprotect */
		{0},
	3, 0, msync,			/* 137 = msync */
		{0},
	3, 0, madvise,			/* 138 = madvise */
		{0},
	3, 0, pagelock,			/* 139 = mpin, munpin */
		{0},
	0, 0, getpagesize,		/* 140 = getpagesize */
		{0},

	4, 0, quotactl,			/* 141 = quotactl */
		{0},
	0, 0, nosys,			/* 142 = */
		{0},
	1, 0, BSDgetpgrp,		/* 143 = BSD style getpgrp */
		{0},
	2, 0, BSDsetpgrp,		/* 144 = BSD style setpgrp */
		{0},
	0, 0, vhangup,			/* 145 = BSD vhangup	*/
		{0},
	1, 0, fsync,			/* 146 = BSD fsync	*/
		{0},
	1, 0, fchdir,			/* 147 = BSD fchdir	*/
		{0},
	2, 0, getrlimit,		/* 148 = getrlimit */
		{0},
	2, 0, setrlimit,		/* 149 = setrlimit */
		{0},
	3, 0, cacheflush,		/* 150 = cacheflush */
		{0},
	3, 0, cachectl,			/* 151 = cachectl */
		{0},
	3, 0, fchown,			/* 152 = fchown */
		{0},
	2, 0, fchmod,			/* 153 = fchmod */
		{0},
	0, 0, nosys,			/* 154 = IRIX4 wait3 */ 
		{0},

/* more 4.3BSD-compatible socket/protocol system calls */
	4, 0, socketpair,		/* 155 =  socketpair */
		__arg_socketpair,

/* new SVR4 system calls */
	3, 0, systeminfo,               /* 156 = systeminfo */
		{0},
	1, 0, uname,               	/* 157 = uname */
		{0},
        3, 0, xstat,			/* 158 = xstat */
		__arg_string2,
        3, 0, lxstat,			/* 159 = lxstat */
		__arg_string2,
        3, 0, fxstat,			/* 160 = fxstat */
		{0},
        4, 0, xmknod,			/* 161 = xmknod */
		__arg_string2,
/* Posix.1 signal calls */
	4, 0, sigaction,		/* 162 = sigaction */
		irix5_64_arg_sigaction,
	1, 0, sigpending,		/* 163 = sigpending */
		__arg_sigpending,
	3, 0, sigprocmask,		/* 164 = sigprocmask */
		__arg_sigprocmask,
	1, SY_NORESTART, sigsuspend,	/* 165 = sigsuspend */
		__arg_sigsuspend,
	3, 0, sigpoll_sys,		/* 166 = sigpoll */
		irix5_64_arg_sigpoll,
	2, 0, swapctl,			/* 167 = swapctl */
		{0},
	1, 0, getcontext,		/* 168 = getcontext */
		{0},
	1, SY_FULLRESTORE, setcontext,	/* 169 = setcontext */
		{0},
	5, 0, waitsys_wrapper,		/* 170 = waitsys */
		irix5_64_arg_waitsys,
	2, 0, sigstack,			/* 171 = sigstack */
		{0},
	2, 0, sigaltstack,		/* 172 = sigaltstack */
		{0},
	2, 0, sigsendsys,		/* 173 = sigsendset */
		__arg_sigsendset,
	2, 0, statvfs,			/* 174 = statvfs */
		__arg_string1,
	2, 0, fstatvfs,			/* 175 = fstatvfs */
		{0},
	5, 0, getpmsg,			/* 176 = getpmsg */
		{0},
	5, 0, putpmsg,			/* 177 = putpmsg */
		{0},
	3, 0, lchown,			/* 178 = lchown */
		__arg_string1,
	0, 0, nopkg,			/* 179 = priocntl */ 
		{0},
	4, 0, sigqueue_sys,		/* 180 = posix.4 D12 sigqueue */
		{0},
	3, 0, readv,        		/* 181 = readv */
		__arg_rwv,
	3, 0, writev,        		/* 182 = writev */
		__arg_rwv,
	0, 0, nosys,			/* 183 = truncate64 */
		{0},
	0, 0, nosys,			/* 184 = ftruncate64 */
		{0},
	0, 0, nosys,			/* 185 = mmap64 */
		{0},
	0, 0, nosys,			/* 186 = dmi */
		{0},
	4, 0, pread,			/* 187 = pread */
		__arg_read,
	4, 0, pwrite,			/* 188 = pwrite */
		__arg_write,
	1, 0, fdatasync,		/* 189 = POSIX fdatasync */
		{0},	
	6, 0, nosys,			/* 190 = was sgifastpath */
		{0},

	5, 0, attr_get,			/* 191 = attr_get */
		__arg_attr_get,
	5, 0, attr_getf,		/* 192 = attr_getf */
		__arg_attr_getf,
	5, 0, attr_set,			/* 193 = attr_set */
		__arg_attr_set,
	5, 0, attr_setf,		/* 194 = attr_setf */
		__arg_attr_setf,
	3, 0, attr_remove,		/* 195 = attr_remove */
		__arg_string12,
	3, 0, attr_removef,		/* 196 = attr_removef */
		__arg_string2,
	5, 0, attr_list,		/* 197 = attr_list */
		{0},
	5, 0, attr_listf,		/* 198 = attr_listf */
		{0},
	4, 0, attr_multi,		/* 199 = attr_multi */
		__arg_string1,
	4, 0, attr_multif,		/* 200 = attr_multif */
		{0},
	0, 0, nosys,			/* 201 = statvfs64 */
		{0},
	0, 0, nosys,			/* 202 = fstatvfs64 */
		{0},
	2, 0, getmountid,		/* 203 = getmountid */
		__arg_string1,

/* new sproc */	
	5, SY_SPAWNER, nsproc,		/* 204 = nsproc */
		{0},
	3, 0, getdents64,		/* 205 = getdents64 */
		{0},
	6, 0, afs_syscall,		/* 206 = reserved for DFS */
		{0},
	4, 0, ngetdents,		/* 207 = ngetdents */
		{0},
	4, 0, ngetdents64,		/* 208 = getdents64 */
		{0},
	8, 0, sgi_sesmgr,	/* 209 = Trix Session Manager */
		{0},
#ifdef CKPT
	6, SY_SPAWNER, pidsprocsp,	/* 210 = pidsprocsp */
		{0},
#else /* CKPT */
	0, 0, nosys,			/* 210 = */
		{0},
#endif /* CKPT */
	4, 0, rexec,			/* 211 = rexec */
		__arg_string2,
	3, 0, timer_create,		/* 212 = timer_create */
		{0},
	1, 0, timer_delete,		/* 213 = timer_delete */
		{0},
	4, 0, timer_settime,		/* 214 = timer_settime */
		{0},
	2, 0, timer_gettime,		/* 215 = timer_gettime */
		{0},
	1, 0, timer_getoverrun,		/* 216 = timer_getoverrun */
		{0},	
	2, 0, sched_rr_get_interval,	/* 217 = sched_rr_get_interval */
		irix5_64_arg_sched_rr_get_interval,
	0, 0, sched_yield,		/* 218 = sched_yield */
		{0},
	1, 0, sched_getscheduler,	/* 219 = sched_getscheduler */
		{0},
	3, 0, sched_setscheduler,	/* 220 = sched_setscheduler */
		{0},
	2, 0, sched_getparam,		/* 221 = sched_getparam */
		{0},
	2, 0, sched_setparam,		/* 222 = sched_setparam */
		{0},
	2, 0, usync_cntl,		/* 223 = usync_cntl */
		{0},
	5, 0, psema_cntl,		/* 224 = psema_cntl */
		__arg_string2,
#ifdef CKPT
	3, SY_FULLRESTORE, ckpt_restartreturn,
		{0},			/* 225 = restart return */
#else /* CKPT */
	0, 0, nosys,			/* 225 = RESERVED for CKPT */
		{0},
#endif /* CKPT */
	5, 0, sysget,			/* 226 = sysget */
		{0},
	3, 0,	xpg4_recvmsg,		/* 227 =  xpg4_recvmsg */
		{0},

	5, 0,	umfscall,		/* 228 =  umfscall */
		__arg_umfscall,
#ifdef CKPT
	5, 0,	nsproctid,		/* 229 = nsproctid */
		{0},
#else /* CKPT */
	0, 0, nosys,			/* 229 = RESERVED for CKPT */
		{0},
#endif /* CKPT */

#ifdef CELL_IRIX
	2, 0, rexec_complete,		/* 230 = rexec_complete */
		{0},
#else /* CELL_IRIX */
	2, 0, nosys,			/* 230 = RESERVED for CELL */
		{0},
#endif /* CELL_IRIX */
	2, 0, xpg4_sigaltstack,		/* 231 = xpg4_sigaltstack */
		{0},
	5, 0,	xpg4_select,		/* 232 = xpg4_select */
		irix5_64_arg_select,
	2, 0, xpg4_setregid,		/* 233 = xpg4_setregid */
		{0},
	2, 0, linkfollow,		/* 234 = linkfollow */
		__arg_string12,
};
#endif	/* _ABI64 */

/* It is unfortunate that these ABI numbers grow in powers of 2 - but
 * this structure is small anyway.
 */
struct syscallsw syscallsw[_MAX_ABI + 1] = {
	{ 0, 0 },
	{ 0, 0 },

	/* ABI_IRIX5 */
	{ sysent, sizeof(sysent) / sizeof(sysent[0]) },

	{ 0, 0 },

#if _MIPS_SIM == _ABI64
	/* ABI_IRIX5_64 */
	{ irix5_64_sysent,
	   sizeof(irix5_64_sysent) / sizeof(irix5_64_sysent[0]) },
#else
	{ 0, 0 },
#endif

	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },

	/* ABI_IRIX5_N32 - shares sysent table with ABI_IRIX5 */
	{ 0, 0 }
};
