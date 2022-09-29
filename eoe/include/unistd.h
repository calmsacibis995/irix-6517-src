#ifndef __UNISTD_H__
#define __UNISTD_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.137 $"
/*
*
* Copyright 1992-1993, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#include <standards.h>
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) || defined(_LANGUAGE_ASSEMBLY)

/* Symbolic constants for the "access" routine: */
#ifndef F_OK
#define	R_OK	004	/* Test for Read permission */
#define	W_OK	002	/* Test for Write permission */
#define	X_OK	001	/* Test for eXecute permission */
#define	F_OK	000	/* Test for existence of File */
#endif

#if _SGIAPI
#define EFF_ONLY_OK 	010	/* Test using effective ids */
#define EX_OK		020	/* Test for Regular, executable file */
#endif	/* _SGIAPI */
#endif /* _LANGUAGE_C  _LANGUAGE_C_PLUS_PLUS _LANGUAGE_ASSEMBLY */

#if _XOPEN4UX || _XOPEN5
#define F_ULOCK	0	/* Unlock a previously locked region */
#define F_LOCK	1	/* Lock a region for exclusive use */
#define F_TLOCK	2	/* Test and lock a region for exclusive use */
#define F_TEST	3	/* Test a region for other processes locks */
#endif /* _XOPEN4UX || _XOPEN5 */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) || defined(_LANGUAGE_ASSEMBLY)

/* _daemonize(3C) flags */
#define	_DF_NOFORK	0x1	/* don't fork */
#define	_DF_NOCHDIR	0x2	/* don't chdir to / */
#define	_DF_NOCLOSE	0x4	/* close no files */

/* Symbolic constants for the "lseek" routine: */
#ifndef SEEK_SET                /* also defined in stdio.h */
#define	SEEK_SET	0	/* Set file pointer to "offset" */
#define	SEEK_CUR	1	/* Set file pointer to current plus "offset" */
#define	SEEK_END	2	/* Set file pointer to EOF plus "offset" */
#endif /* !SEEK_SET */

#if _SGIAPI
/* Path names: */
#define	GF_PATH	"/etc/group"	/* Path name of the "group" file */
#define	PF_PATH	"/etc/passwd"	/* Path name of the "passwd" file */
#endif /* _SGI_API */

/* compile-time symbolic constants,
** Support does not mean the feature is enabled.
** Use pathconf/sysconf to obtain actual configuration value.
** 
*/

#define _POSIX_JOB_CONTROL	1
#define _POSIX_SAVED_IDS	1

#if _POSIX93 || _XOPEN5

#define _XOPEN_REALTIME		1

#define _POSIX_ASYNCHRONOUS_IO	1
#define _POSIX_FSYNC		1
#define _POSIX_MAPPED_FILES	1
#define _POSIX_MEMLOCK		1
#define _POSIX_MEMLOCK_RANGE	1
#define _POSIX_MEMORY_PROTECTION	1
#define _POSIX_MESSAGE_PASSING	1
#define _POSIX_PRIORITY_SCHEDULING	1
#define _POSIX_REALTIME_SIGNALS	1
#define _POSIX_SEMAPHORES		1
#define _POSIX_SHARED_MEMORY_OBJECTS	1
#define _POSIX_SYNCHRONIZED_IO	1
#define _POSIX_TIMERS		1
#endif /* _POSIX93 || _XOPEN5 */

#if _LFAPI
#define _LFS_ASYNCHRONOUS_IO 1
#define _LFS_LARGEFILE 1
#define _LFS64_ASYNCHRONOUS_IO 1
#define _LFS64_LARGEFILE 1
#define _LFS64_STDIO 1
#endif /* _LFAPI */

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE		0
#endif

#ifndef	NULL
#define NULL	0L
#endif

#define	STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

/* Current version of POSIX */
#ifndef _POSIX_VERSION
#define _POSIX_VERSION	199506L
#endif

/* Current version of XOPEN */
#ifndef _XOPEN_VERSION
#define _XOPEN_VERSION 4
#endif

#ifndef _POSIX2_C_VERSION
#define	_POSIX2_C_VERSION	199209L
#endif

#ifndef _POSIX2_VERSION
#define	_POSIX2_VERSION		199209L
#endif

#ifndef _XOPEN_XCU_VERSION
#define	_XOPEN_XCU_VERSION	4
#endif

#ifndef _XOPEN_XPG4
#define	_XOPEN_XPG4		1
#endif

#ifndef _XOPEN_UNIX
#define	_XOPEN_UNIX		1
#endif

#ifndef _POSIX2_C_BIND
#define	_POSIX2_C_BIND		1
#endif

#ifndef _POSIX2_LOCALEDEF
#define	_POSIX2_LOCALEDEF	1
#endif

#ifndef _POSIX2_C_DEV
#define	_POSIX2_C_DEV		1
#endif

#ifndef _POSIX2_CHAR_TERM
#define	_POSIX2_CHAR_TERM	1
#endif

#ifndef _POSIX2_FORT_DEV
#define	_POSIX2_FORT_DEV	1
#endif

#ifndef _POSIX2_FORT_RUN
#define	_POSIX2_FORT_RUN	1
#endif

#ifndef _POSIX2_SW_DEV
#define	_POSIX2_SW_DEV		1
#endif

#ifndef _POSIX2_UPE
#define	_POSIX2_UPE		1
#endif

#ifndef _XOPEN_ENH_I18N
#define	_XOPEN_ENH_I18N		1
#endif

#if _XOPEN5
#ifndef _XOPEN_SHM
#define	_XOPEN_SHM		1
#endif
#endif

/* command names for XPG4/POSIX2 confstr */
#define _CS_PATH                1
#define _CS_HOSTNAME            2       /* name of node */
#define _CS_RELEASE             3       /* release of operating system */
#define _CS_VERSION             4       /* version field of utsname */
#define _CS_MACHINE             5       /* kind of machine */
#define _CS_ARCHITECTURE        6       /* instruction set arch */
#define _CS_HW_SERIAL           7       /* hardware serial number */
#define _CS_HW_PROVIDER         8       /* hardware manufacturer */
#define _CS_SRPC_DOMAIN         9       /* secure RPC domain */
#define _CS_INITTAB_NAME        10      /* name of inittab file used */
#define _CS_SYSNAME             11      /* name of operating system */
#if _LFAPI
#define _CS_LFS_CFLAGS		68
#define _CS_LFS_LDFLAGS		69
#define _CS_LFS_LIBS		70
#define _CS_LFS_LINTFLAGS	71
#define _CS_LFS64_CFLAGS	72
#define _CS_LFS64_LDFLAGS	73
#define _CS_LFS64_LIBS		74
#define _CS_LFS64_LINTFLAGS	75
#endif
#if _XOPEN5 || _SGIAPI
#define	_CS_XBS5_ILP32_OFF32_CFLAGS	76
#define	_CS_XBS5_ILP32_OFF32_LDFLAGS	77
#define	_CS_XBS5_ILP32_OFF32_LIBS	78
#define	_CS_XBS5_ILP32_OFF32_LINTFLAGS	79

#define	_CS_XBS5_ILP32_OFFBIG_CFLAGS	80
#define	_CS_XBS5_ILP32_OFFBIG_LDFLAGS	81
#define	_CS_XBS5_ILP32_OFFBIG_LIBS	82
#define	_CS_XBS5_ILP32_OFFBIG_LINTFLAGS	83

#define	_CS_XBS5_LP64_OFF64_CFLAGS	84
#define	_CS_XBS5_LP64_OFF64_LDFLAGS	85
#define	_CS_XBS5_LP64_OFF64_LIBS	86
#define	_CS_XBS5_LP64_OFF64_LINTFLAGS	87

#define	_CS_XBS5_LPBIG_OFFBIG_CFLAGS	88
#define	_CS_XBS5_LPBIG_OFFBIG_LDFLAGS	89
#define	_CS_XBS5_LPBIG_OFFBIG_LIBS	90
#define	_CS_XBS5_LPBIG_OFFBIG_LINTFLAGS	91

#endif
#if _ABIAPI || _SGIAPI
#define _MIPS_CS_BASE		100
#define _MIPS_CS_VENDOR		(_MIPS_CS_BASE+0)
#define _MIPS_CS_OS_PROVIDER	(_MIPS_CS_BASE+1)
#define _MIPS_CS_OS_NAME	(_MIPS_CS_BASE+2)
#define _MIPS_CS_HW_NAME	(_MIPS_CS_BASE+3)
#define _MIPS_CS_NUM_PROCESSORS	(_MIPS_CS_BASE+4)
#define _MIPS_CS_HOSTID		(_MIPS_CS_BASE+5)
#define _MIPS_CS_OSREL_MAJ	(_MIPS_CS_BASE+6)
#define _MIPS_CS_OSREL_MIN	(_MIPS_CS_BASE+7)
#define _MIPS_CS_OSREL_PATCH	(_MIPS_CS_BASE+8)
#define _MIPS_CS_PROCESSORS	(_MIPS_CS_BASE+9)
#define _MIPS_CS_AVAIL_PROCESSORS	(_MIPS_CS_BASE+10)
#define _MIPS_CS_SERIAL		(_MIPS_CS_BASE+11)
#endif

#if _POSIX1C
/*
 * POSIX1C options
 */
#define _POSIX_THREADS				1
#define _POSIX_THREAD_SAFE_FUNCTIONS		1
#define _POSIX_THREAD_PRIORITY_SCHEDULING	1
#define _POSIX_THREAD_ATTR_STACKADDR		1
#define _POSIX_THREAD_ATTR_STACKSIZE		1
#define _POSIX_THREAD_PRIO_INHERIT		1
#define _POSIX_THREAD_PRIO_PROTECT		1
#define _POSIX_THREAD_PROCESS_SHARED		1
#endif

#if _SGIAPI || _XOPEN5
/*
 * XPG/5 Options
 */
#define	_XBS5_ILP32_OFF32	1
#define	_XBS5_ILP32_OFFBIG	1
/* don't define _XBS5_LP64_OFF64 and _XBS5_LPBIG_OFFBIG */
#endif

#include <sys/types.h>
#include <sys/unistd.h>

/*
 * POSIX 1003.1 Functions
 */
extern int access(const char *, int);
extern unsigned alarm(unsigned);
extern int chdir(const char *);
extern int chown(const char *, uid_t, gid_t);
extern int close(int);
extern char *ctermid(char *);
extern char *cuserid(char *);
extern int dup(int);
extern int dup2(int, int);
extern int execl(const char *, const char *, ...);
extern int execle(const char *, const char *, ...);
extern int execlp(const char *, const char *, ...);
extern int execv(const char *, char *const *);
extern int execve(const char *, char *const *, char *const *);
extern int execvp(const char *, char *const *);
extern void _exit(int);
extern pid_t fork(void);
extern long fpathconf(int, int);

extern char *getcwd(char *, size_t);    /* POSIX flavor of getcwd */
extern gid_t getegid(void);
extern uid_t geteuid(void);
extern gid_t getgid(void);
#if defined(_BSD_COMPAT)
extern int getgroups(int, int *);
#else
extern int getgroups(int, gid_t *);
#endif /* _BSD_COMPAT */
extern char *getlogin(void);
#if defined(_BSD_COMPAT)
extern int getpgrp(int);
#else
extern pid_t getpgrp(void);
#endif
extern pid_t getpid(void);
extern pid_t getppid(void);
extern uid_t getuid(void);
extern int isatty(int);
#if _SGIAPI || _ABIAPI
extern int link(const char *, const char *);
extern int linkfollow(const char *, const char *);
#else
/* hack to get the slightly different semantics of posix/xpg link. */
extern int _linkfollow(const char *, const char *);

/*REFERENCED*/
static int
link(const char *__from, const char *__to)
{
	return _linkfollow(__from, __to);
}
#endif	/* _SGIAPI || _ABIAPI */
extern off_t lseek(int, off_t, int);
extern long pathconf(const char *, int);
extern int pause(void);
extern int pipe(int *);
extern ssize_t read(int, void *, size_t);
extern int rmdir(const char *);
extern int setgid(gid_t);
extern int setpgid(pid_t, pid_t);
extern pid_t setsid(void);
extern int setuid(uid_t);
extern unsigned sleep(unsigned);
extern long sysconf(int);
extern pid_t tcgetpgrp(int);
extern int tcsetpgrp(int, pid_t);
extern char *ttyname(int);
extern int unlink(const char *);
extern ssize_t write(int, const void *, size_t);

#if _POSIX93
/*
 * POSIX 1003.1b additions
 */
extern int fdatasync(int);
#endif /*  _POSIX93 */

#if _POSIX93 || _XOPEN4
/* 
 * POSIX 1003.1b functions that are also part of XPG4
 */
extern int fsync(int);
#endif /*  _POSIX93 || _XOPEN4 */

#if _POSIX93 || _XOPEN4UX
/* 
 * POSIX 1003.1b functions that are also part of XPG4-UX
 */
extern int ftruncate(int, off_t);	
#endif

#if _POSIX1C
/*
 * 1003.1c additions
 */
extern int getlogin_r(char *, size_t);
extern int ttyname_r(int, char *, size_t);
extern int pthread_atfork(void (*)(void), void (*)(void), void (*)(void));
#endif

#if _POSIX2
/*
 * POSIX.2 additions
 */
#include <getopt.h>
extern size_t   confstr(int, char *, size_t);
#endif /* _POSIX2 */

#if _XOPEN4
/*
 * XPG4 additions
 */
extern int chroot(const char *);
extern int nice(int);
extern char     *crypt(const char *, const char *);
extern void     encrypt(char *, int);
extern char     *getpass(const char *);
extern void     swab(const void *, void *, ssize_t);
#endif /* _XOPEN4 */

#if _XOPEN4UX
/*
 * XPG4-UX additions
 */
extern int brk(void *);
extern int fchown(int, uid_t, gid_t);
extern int fchdir(int);
extern int getdtablesize(void);
extern long gethostid(void);
extern int gethostname(char *, size_t);
extern int getpagesize(void);
extern pid_t getpgid(pid_t);
extern pid_t getsid(pid_t);
extern char *getwd(char *);
extern int lchown(const char *, uid_t, gid_t);
extern int lockf(int, int, off_t);
extern int readlink(const char *, char *, size_t);
extern void *sbrk(ssize_t);
#if defined(_BSD_COMPAT)
extern int setpgrp(int, int);
#else	/* !_BSD_COMPAT */
extern pid_t setpgrp(void);
#endif /* _BSD_COMPAT */

#if _NO_XOPEN4 || _ABIAPI
extern int setregid(gid_t, gid_t);
#else
/* hack to get the slightly different semantics of xpg4 setregid. */
extern int __xpg4_setregid(gid_t, gid_t);

/*REFERENCED*/
static int
setregid(gid_t rgid, gid_t egid)
{
	return __xpg4_setregid(rgid, egid);
}
#endif	/* _NO_XOPEN4 */
extern int setreuid(uid_t, uid_t);
extern int symlink(const char *, const char *);
extern void sync(void);
extern int truncate(const char *, off_t);
extern useconds_t ualarm(useconds_t, useconds_t);
extern int usleep(useconds_t);
#if !_SGIAPI
/* Only for XPG4-UX/ABI since IRIX vfork == fork */
extern pid_t _vfork(void);
/* REFERENCED */
static pid_t vfork(void) { return _vfork(); }
#endif	/* !_SGIAPI */
#endif /* _XOPEN4UX */
#if _XOPEN5
#if (_MIPS_SIM == _ABIO32) && !defined(_SGI_COMPILING_LIBC)
ssize_t __pread32(int, void *, size_t, off_t);
ssize_t __pwrite32(int, const void *, size_t, off_t);
#define pread __pread32
#define pwrite __pwrite32
#else
extern ssize_t pread(int, void *, size_t, off_t);
extern ssize_t pwrite(int, const void *, size_t, off_t);
#endif /* _MIPS_SIM == _ABIO32 */
#endif /*  _XOPEN5 */
/*
 * All other additions go here. These are non-POSIX/XOPEN
 */

#if _SGIAPI || defined(_BSD_TYPES) || defined(_BSD_COMPAT)
/* Need to use the same predicate as
 * types.h for inclusion of bsd_types.h/select.h
 */
struct timeval;
extern int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#endif

/*
 * All SGI specific and non POSIX/XOPEN orphaned calls go here
 */
#if _ABIAPI || _SGIAPI
#include <sys/uio.h>
#include <utime.h>	/* 4.0 compatibility */
#include <re_comp.h>
extern int acct(const char *);
extern void exit(int);
extern char *gettxt(const char *, const char *);
extern int profil(unsigned short *, unsigned int, unsigned int, unsigned int);
extern int ptrace(int, pid_t, int, int);
extern int rename(const char *, const char *);	/* added in 1003.1b */
extern int seteuid(uid_t);
extern int setegid(gid_t);
extern int stime(const time_t *);
extern off_t tell(int);
#endif 	/* _ABIAPI || _SGIAPI */

#if _SGIAPI
struct acct_spi;	/* needed by get/setspinfo */
extern int atfork_child(void (*func)(void));
extern int atfork_child_prepend(void (*func)(void));
extern int atfork_parent(void (*func)(int, int));
extern int atfork_pre(void (*func)(void));
extern int atsproc_child(void (*func)(void));
extern int atsproc_parent(void (*func)(int, int));
extern int atsproc_pre(void (*func)(unsigned int));
extern void bset(char *, bitnum_t);
extern void bclr(char *, bitnum_t);
extern int btst(char *, bitnum_t);
extern void bfset(char *, bitnum_t, bitlen_t);
extern void bfclr(char *, bitnum_t, bitlen_t);
extern bitlen_t bftstset(char *, bitnum_t, bitlen_t);
extern bitlen_t bftstclr(char *, bitnum_t, bitlen_t);
extern int BSDchown(const char *, uid_t, gid_t);
extern int BSDdup2(int, int);
extern int BSDfchown(int, uid_t, gid_t);
extern int BSDgetpgrp(int);
extern int BSDsetpgrp(int, int);
extern int BSDsetgroups(int, int *);
extern int BSDgetgroups(int, int *);
extern int _daemonize(int, int, int, int);
extern ash_t getash(void);
extern int getdtablehi(void);
extern int getdomainname(char *, int);
extern prid_t getprid(void);
extern char *_getpty(int *, int, mode_t, int);
extern int getspinfo(struct acct_spi *);
extern int mincore(caddr_t, size_t, char *);
extern int mpin(void *, size_t);
extern int munpin(void *, size_t);
extern int newarraysess(void);
extern pid_t pcreatel(const char *, const char *, ...);
extern pid_t pcreatelp(const char *, const char *, ...);
extern pid_t pcreatev(const char *, char *const *);
extern pid_t pcreateve(const char *, char *const *, char *const *);
extern pid_t pcreatevp(const char *, char *const *);
#if !defined(_SGI_COMPILING_LIBC)
extern ssize_t pread64(int, void *, size_t, off64_t);
extern ssize_t pwrite64(int, const void *, size_t, off64_t);
#endif
extern int rexecl(cell_t, const char *, const char *, ...);
extern int rexecle(cell_t, const char *, const char *, ...);
extern int rexeclp(cell_t, const char *, const char *, ...);
extern int rexecv(cell_t, const char *, char *const *);
extern int rexecve(cell_t, const char *, char *const *, char *const *);
extern int rexecvp(cell_t, const char *, char *const *);
extern float _sqrt_s(float);
extern double _sqrt_d(double);
extern int setash(ash_t);
extern int setdomainname(const char *, int);
#if defined(_BSD_COMPAT)
extern int setgroups(int, int *);
#else	/* !_BSD_COMPAT */
extern int setgroups(int, const gid_t *);
#endif /* _BSD_COMPAT */
extern int sethostid(int);
extern int sethostname(const char *, int);
extern int setprid(prid_t);
extern int setrgid(gid_t);
extern int setruid(uid_t);
extern int setspinfo(struct acct_spi *);
extern int sgikopt(const char *, char *, int);
extern long sginap(long);
extern off64_t tell64(int);
extern int vhangup(void);

/*
 * These are defined in XPG4 in stropts.h but are needed here
 * as well - but protected from polluting XPG4 namespace.
 */
extern int fattach(int, const char *);
extern int fdetach(const char *);
extern int ioctl(int, int, ...);

#endif /* _SGIAPI */

#if _LFAPI
	/* Large file additions */
extern int ftruncate64(int, off64_t);
extern int lockf64(int, int, off64_t);
extern off64_t lseek64(int, off64_t, int);
extern int truncate64(const char *, off64_t);
#endif 	/* _LFAPI */

#endif /* _LANGUAGE_C  _LANGUAGE_C_PLUS_PLUS  _LANGUAGE_ASSEMBLY */

#ifdef __cplusplus
}
#endif
#endif /* !__UNISTD_H__ */
