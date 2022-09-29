/*
 * Copyright 1990, Silicon Graphics, Inc. 
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

#define _KMEMUSER

#ifdef __STDC__
	#pragma weak sys_call = _sys_call
#endif

#include "synonyms.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/sysmips.h>
#include <sys/sysmp.h>
#include <sys/syssgi.h>
#include <sys/uadmin.h>
#include <sys/mac_label.h>
#include <sys/sat.h>
#include <sys/ulimit.h>
#include <sys.s>
#include <string.h>


/*
 * Decode system call number into a name.
 * Accurate as of the following revisions:
 *	os/sysent.c:	rev 3.83
 *	sys/syssgi.h:	rev 3.57
 *	sys/sysmips.h:	rev 3.10
 *	sys/sysmp.h:	rev 3.38
 *	sys/fcntl.h:	rev 3.32
 */
#define	MAXSYSNAME	16

/* move out of function scope so we get a global symbol for use with data cording */
static char *buffer = NULL;

char *
sys_call(int major, int minor)
{
	char number[10];
	static const char sysname[][MAXSYSNAME] = {
	    "nosys", "exit", "fork", "read", "write", "open", "close",
	    "wait", "creat", "link", "unlink", "exec", "chdir", "time",
	    "mknod", "chmod", "chown", "brk", "stat", "lseek", "getpid",
	    "mount", "umount", "setuid", "getuid", "stime", "ptrace",
	    "alarm", "fstat", "pause", "utime", "old stty", "old gtty",
	    "access", "nice", "statfs", "sync", "kill", "fstatfs", "setpgrp",
	    "syssgi", "dup", "pipe", "times", "prof", "lock", "setgid",
	    "getgid", "ssig", "msgsys", "sysmips", "acct",
	    "shmsys", "semsys", "ioctl",
	    "uadmin", "sysmp", "utssys", "nosys", "exece",
	    "umask", "chroot", "fcntl", "ulimit", "nosys", "nosys",
	    "nosys", "nosys", "nosys", "nosys", "nosys", "nosys",
	    "nosys", "nosys", "nosys", "getrlimit64", "setrlimit64",
	    "nanosleep", "lseek64", "rmdir", "mkdir", "getdents", "sginap",
	    "sgikopt", "sysfs", "getmsg", "putmsg", "poll",
	    "sigreturn", "accept", "bind", "connect",
	    "gethostid", "getpeername", "getsockname", "getsockopt",
	    "listen", "recv", "recvfrom", "recvmsg", "select", "send",
	    "sendmsg", "sendto", "sethostid", "setsockopt", "shutdown",
	    "socket", "gethostname", "sethostname", "getdomainname",
	    "setdomainname", "truncate", "ftruncate", "rename",
	    "symlink", "readlink", "lstat",
	    "nosys", "nfs_svc", "nfs_getfh", "async_daemon",
	    "exportfs", "setregid", "setreuid",
	    "getitimer", "setitimer", "adjtime", "gettimeofday", "sproc",
	    "prctl", "procblk", "old texturebind", "old sgigsc", "mmap",
	    "munmap", "mprotect", "msync", "madvise", "mpin/munpin",
	    "getpagesize", "quotactl", "nosys", "BSDgetpgrp", "BSDsetpgrp",
	    "vhangup", "fsync", "fchdir", "getrlimit",
	    "setrlimit", "cacheflush", "cachectl", "fchown", "fchmod",
	    "wait3", "socketpair", "systeminfo", "uname", "xstat", "lxstat",
	    "fxstat", "xmknod", "sigaction", "sigpending", "sigprocmask",
	    "sigsuspend", "sigpoll", "swapctl", "getcontext", "setcontext",
	    "waitsys", "sigstack", "sigaltstack", "sigsendset", "statvfs",
	    "fstatvfs", "getpmsg", "putpmsg", "lchown", "priocntl",
	    "sigqueue", "readv", "writev", "truncate64", "ftruncate64",
	    "mmap64" };

	if (buffer == NULL)
		buffer = malloc(256);

	switch (major+SYSVoffset) {

	    case SYS_syssgi:
		strcpy( buffer, sysname[major] );
		strcat( buffer, "(" );
		switch (minor) {
		    case SGI_SYSID:
			strcat(buffer, "SGI_SYSID");
			break;
		    case SGI_TUNE:
			strcat(buffer, "SGI_TUNE");
			break;
		    case SGI_IDBG:
			strcat(buffer, "SGI_IDBG");
			break;
		    case SGI_INVENT:
			strcat(buffer, "SGI_INVENT");
			break;
		    case SGI_RDNAME:
			strcat(buffer, "SGI_RDNAME");
			break;
		    case SGI_SETLED:
			strcat(buffer, "SGI_SETLED");
			break;
		    case SGI_SETNVRAM:
			strcat(buffer, "SGI_SETNVRAM");
			break;
		    case SGI_GETNVRAM:
			strcat(buffer, "SGI_GETNVRAM");
			break;
		    case SGI_QUERY_FTIMER:
			strcat(buffer, "SGI_QUERY_FTIMER");
			break;
		    case SGI_QUERY_CYCLECNTR:
			strcat(buffer, "SGI_QUERY_CYCLECNTR");
			break;
		    case SGI_SETSID:
			strcat(buffer, "SGI_SETSID");
			break;
		    case SGI_SETPGID:
			strcat(buffer, "SGI_SETPGID");
			break;
		    case SGI_SYSCONF:
			strcat(buffer, "SGI_SYSCONF");
			break;
		    case SGI_PATHCONF:
			strcat(buffer, "SGI_PATHCONF");
			break;
		    case SGI_READB:
			strcat(buffer, "SGI_READB");
			break;
		    case SGI_WRITEB:
			strcat(buffer, "SGI_WRITEB");
			break;
		    case SGI_SETGROUPS:
			strcat(buffer, "SGI_SETGROUPS");
			break;
		    case SGI_GETGROUPS:
			strcat(buffer, "SGI_GETGROUPS");
			break;
		    case SGI_SETTIMEOFDAY:
			strcat(buffer, "SGI_SETTIMEOFDAY");
			break;
		    case SGI_SETTIMETRIM:
			strcat(buffer, "SGI_SETTIMETRIM");
			break;
		    case SGI_GETTIMETRIM:
			strcat(buffer, "SGI_GETTIMETRIM");
			break;
		    case SGI_SPROFIL:
			strcat(buffer, "SGI_SPROFIL");
			break;
		    case SGI_RUSAGE:
			strcat(buffer, "SGI_RUSAGE");
			break;
		    case SGI_SIGSTACK:
			strcat(buffer, "SGI_SIGSTACK");
			break;
		    case SGI_NETPROC:
			strcat(buffer, "SGI_NETPROC");
			break;
		    case SGI_SIGALTSTACK:
			strcat(buffer, "SGI_SIGALTSTACK");
			break;
		    case SGI_BDFLUSHCNT:
			strcat(buffer, "SGI_BDFLUSHCNT");
			break;
		    case SGI_SSYNC:
			strcat(buffer, "SGI_SSYNC");
			break;
		    case SGI_NFSCNVT:
			strcat(buffer, "SGI_NFSCNVT");
			break;
		    case SGI_GETPGID:
			strcat(buffer, "SGI_GETPGID");
			break;
		    case SGI_GETSID:
			strcat(buffer, "SGI_GETSID");
			break;
		    case SGI_IOPROBE:
			strcat(buffer, "SGI_IOPROBE");
			break;
		    case SGI_CONFIG:
			strcat(buffer, "SGI_CONFIG");
			break;
		    case SGI_ELFMAP:
			strcat(buffer, "SGI_ELFMAP");
			break;
		    case SGI_MCONFIG:
			strcat(buffer, "SGI_MCONFIG");
			break;
		    case SGI_GETPLABEL:
			strcat(buffer, "SGI_GETPLABEL");
			break;
		    case SGI_SETPLABEL:
			strcat(buffer, "SGI_SETPLABEL");
			break;
		    case SGI_GETLABEL:
			strcat(buffer, "SGI_GETLABEL");
			break;
		    case SGI_SETLABEL:
			strcat(buffer, "SGI_SETLABEL");
			break;
		    case SGI_SATREAD:
			strcat(buffer, "SGI_SATREAD");
			break;
		    case SGI_SATWRITE:
			strcat(buffer, "SGI_SATWRITE");
			break;
		    case SGI_SATCTL:
			strcat(buffer, "SGI_SATCTL");
			break;
		    case SGI_LOADATTR:
			strcat(buffer, "SGI_LOADATTR");
			break;
		    case SGI_UNLOADATTR:
			strcat(buffer, "SGI_UNLOADATTR");
			break;
		    case SGI_RECVLUMSG:
			strcat(buffer, "SGI_RECVLMSG");
			break;
		    case SGI_PLANGMOUNT:
			strcat(buffer, "SGI_PLANGMOUNT");
			break;
		    case SGI_GETPSOACL:
			strcat(buffer, "SGI_GETPSOACL");
			break;
		    case SGI_SETPSOACL:
			strcat(buffer, "SGI_SETPSOACL");
			break;
		    case SGI_SETASH:
			strcat(buffer, "SGI_SETASH");
			break;
		    case SGI_GETASH:
			strcat(buffer, "SGI_GETASH");
		    	break;
		    case SGI_SETPRID:
			strcat(buffer, "SGI_SETPRID");
			break;
		    case SGI_GETPRID:
			strcat(buffer, "SGI_GETPRID");
			break;
		    case SGI_SETSPINFO:
			strcat(buffer, "SGI_SETSPINFO");
			break;
		    case SGI_GETSPINFO:
			strcat(buffer, "SGI_GETSPINFO");
			break;
		    case SGI_NEWARRAYSESS:
			strcat(buffer, "SGI_NEWARRAYSESS");
			break;
		    case SGI_PROC_ATTR_GET:
			strcat(buffer, "SGI_PROC_ATTR_GET");
			break;
		    case SGI_PROC_ATTR_SET:
			strcat(buffer, "SGI_PROC_ATTR_SET");
			break;
		    case SGI_CAP_GET:
			strcat(buffer, "SGI_CAP_GET");
			break;
		    case SGI_CAP_SET:
			strcat(buffer, "SGI_CAP_SET");
			break;
		    case SGI_MAC_GET:
			strcat(buffer, "SGI_MAC_GET");
			break;
		    case SGI_MAC_SET:
			strcat(buffer, "SGI_MAC_SET");
			break;
		    case SGI_ACL_GET:
			strcat(buffer, "SGI_ACL_GET");
			break;
		    case SGI_ACL_SET:
			strcat(buffer, "SGI_ACL_SET");
			break;
		    default:
			sprintf(number, "%d", minor);
			strcat(buffer, number);
			break;
		}
		strcat( buffer, ")" );
		break;

	    case SYS_msgsys:
		switch (minor & 0x3) {
		    case 0:
			strcpy( buffer, "msgget" );
			break;
		    case 1:
			strcpy( buffer, "msgctl(" );
			switch (minor >> 2) {
			    case IPC_RMID:
				strcat(buffer, "IPC_RMID");
				break;
			    case IPC_SET:
				strcat(buffer, "IPC_SET");
				break;
			    case IPC_STAT:
				strcat(buffer, "IPC_STAT");
				break;
			    default:
				sprintf(number, "%d", minor);
				strcat(buffer, number);
				break;
			}
			strcat(buffer, ")");
			break;
		    case 2:
			strcpy( buffer, "msgrcv" );
			break;
		    case 3:
			strcpy( buffer, "msgsnd" );
			break;
		    default:
			strcpy( buffer, "unknown msgsys operation" );
		}
		break;

	    case SYS_sysmips:
		strcpy( buffer, sysname[major] );
		strcat( buffer, "(" );
		switch (minor) {
		    case SETNAME:
			strcat(buffer, "SETNAME");
			break;
		    case STIME:
			strcat(buffer, "STIME");
			break;
		    case FLUSH_CACHE:
			strcat(buffer, "FLUSH_CACHE");
			break;
		    case 4:		/* defunct SMIPSSWPI */
			strcat(buffer, "SMIPSSWPI");
			break;
		    case MIPS_FPSIGINTR:
			strcat(buffer, "MIPS_FPSIGINTR");
			break;
		    case MIPS_FPU:
			strcat(buffer, "MIPS_FPU");
			break;
		    case MIPS_FIXADE:
			strcat(buffer, "MIPS_FIXADE");
			break;
		    default:
			sprintf(number, "%d", minor);
			strcat(buffer, number);
			break;
		}
		strcat( buffer, ")" );
		break;

	    case SYS_shmsys:
		switch (minor & 0x3) {
		    case 0:
			strcpy( buffer, "shmat" );
			break;
		    case 1:
			strcpy( buffer, "shmctl(" );
			switch (minor >> 2) {
			    case IPC_RMID:
				strcat(buffer, "IPC_RMID");
				break;
			    case IPC_SET:
				strcat(buffer, "IPC_SET");
				break;
			    case IPC_STAT:
				strcat(buffer, "IPC_STAT");
				break;
			    case SHM_LOCK:
				strcat(buffer, "SHM_LOCK");
				break;
			    case SHM_UNLOCK:
				strcat(buffer, "SHM_UNLOCK");
				break;
			    default:
				sprintf(number, "%d", minor);
				strcat(buffer, number);
				break;
			}
			strcat(buffer, ")");
			break;
		    case 2:
			strcpy( buffer, "shmdt" );
			break;
		    case 3:
			strcpy( buffer, "shmget" );
			break;
		    default:
			strcpy( buffer, "unknown shmsys operation" );
		}
		break;

	    case SYS_semsys:
		switch (minor & 0x3) {
		    case 0:
			strcpy( buffer, "semctl(" );
			switch (minor >> 2) {
			    case IPC_RMID:
				strcat(buffer, "IPC_RMID");
				break;
			    case IPC_SET:
				strcat(buffer, "IPC_SET");
				break;
			    case IPC_STAT:
				strcat(buffer, "IPC_STAT");
				break;
			    case GETNCNT:
				strcat(buffer, "GETNCNT");
				break;
			    case GETPID:
				strcat(buffer, "GETPID");
				break;
			    case GETVAL:
				strcat(buffer, "GETVAL");
				break;
			    case GETALL:
				strcat(buffer, "GETALL");
				break;
			    case GETZCNT:
				strcat(buffer, "GETZCNT");
				break;
			    case SETVAL:
				strcat(buffer, "SETVAL");
				break;
			    case SETALL:
				strcat(buffer, "SETALL");
				break;
			    default:
				sprintf(number, "%d", minor);
				strcat(buffer, number);
				break;
			}
			strcat(buffer, ")");
			break;
		    case 1:
			strcpy( buffer, "semget" );
			break;
		    case 2:
			strcpy( buffer, "semop" );
			break;
		    default:
			strcpy( buffer, "unknown semsys operation" );
		}
		break;

	    case SYS_ioctl:
		/* TODO: interpret the minor "command" number */
		sprintf( buffer, "%s,'%c'<<8|%d", sysname[major],
			(minor&0xff00)>>8, minor&0xff );
		break;

	    case SYS_uadmin:
		strcpy( buffer, sysname[major] );
		strcat( buffer, "(" );
		switch (minor) {
		    case A_REBOOT:
			strcat(buffer, "A_REBOOT");
			break;
		    case A_SHUTDOWN:
			strcat(buffer, "A_SHUTDOWN");
			break;
		    case A_REMOUNT:
			strcat(buffer, "A_REMOUNT");
			break;
		    case A_KILLALL:
			strcat(buffer, "A_KILLALL");
			break;
		    default:
			sprintf(number, "%d", minor);
			strcat(buffer, number);
			break;
		}
		strcat( buffer, ")" );
		break;

	    case SYS_sysmp:
		strcpy( buffer, sysname[major] );
		strcat( buffer, "(" );
		switch (minor) {
		    case MP_NPROCS:
			strcat(buffer, "MP_NPROCS");
			break;
		    case MP_NAPROCS:
			strcat(buffer, "MP_NAPROCS");
			break;
		    case MP_ENABLE:
			strcat(buffer, "MP_ENABLE");
			break;
		    case MP_DISABLE:
			strcat(buffer, "MP_DISABLE");
			break;
		    case MP_KERNADDR:
			strcat(buffer, "MP_KERNADDR");
			break;
		    case MP_SASZ:
			strcat(buffer, "MP_SASZ");
			break;
		    case MP_SAGET:
			strcat(buffer, "MP_SAGET");
			break;
		    case MP_SCHED:
			strcat(buffer, "MP_SCHED");
			break;
		    case MP_PGSIZE:
			strcat(buffer, "MP_PGSIZE");
			break;
		    case MP_SAGET1:
			strcat(buffer, "MP_SAGET1");
			break;
		    case MP_EMPOWER:
			strcat(buffer, "MP_EMPOWER");
			break;
		    case MP_RESTRICT:
			strcat(buffer, "MP_RESTRICT");
			break;
		    case MP_CLOCK:
			strcat(buffer, "MP_CLOCK");
			break;
		    case MP_MUSTRUN:
			strcat(buffer, "MP_MUSTRUN");
			break;
		    case MP_RUNANYWHERE:
			strcat(buffer, "MP_RUNANYWHERE");
			break;
		    case MP_STAT:
			strcat(buffer, "MP_STAT");
			break;
		    case MP_ISOLATE:
			strcat(buffer, "MP_ISOLATE");
			break;
		    case MP_UNISOLATE:
			strcat(buffer, "MP_UNISOLATE");
			break;
		    case MP_PREEMPTIVE:
			strcat(buffer, "MP_PREEMPTIVE");
			break;
		    case MP_NONPREEMPTIVE:
			strcat(buffer, "MP_NONPREEMPTIVE");
			break;
		    case MP_FASTCLOCK:
			strcat(buffer, "MP_FASTCLOCK");
			break;
		    default:
			sprintf(number, "%d", minor);
			strcat(buffer, number);
			break;
		}
		strcat( buffer, ")" );
		break;

	    case SYS_fcntl:
		strcpy( buffer, sysname[major] );
		strcat( buffer, "(" );
		switch (minor) {
		    case F_DUPFD:
			strcat(buffer, "F_DUPFD");
			break;
		    case F_GETFD:
			strcat(buffer, "F_GETFD");
			break;
		    case F_SETFD:
			strcat(buffer, "F_SETFD");
			break;
		    case F_GETFL:
			strcat(buffer, "F_GETFL");
			break;
		    case F_SETFL:
			strcat(buffer, "F_SETFL");
			break;
		    case F_GETLK:
			strcat(buffer, "F_GETLK");
			break;
		    case F_SETLK:
			strcat(buffer, "F_SETLK");
			break;
		    case F_SETLKW:
			strcat(buffer, "F_SETLKW");
			break;
		    case F_CHKFL:
			strcat(buffer, "F_CHKFL");
			break;
		    case F_ALLOCSP:	/* irix4 F_GETOWN */
			strcat(buffer, "F_ALLOCSP/F_GETOWN");
			break;
		    case F_FREESP:	/* irix4 F_SETOWN */
			strcat(buffer, "F_FREESP/F_SETOWN");
			break;
		    case F_SETBSDLK:
			strcat(buffer, "F_SETBSDLK");
			break;
		    case F_SETBSDLKW:
			strcat(buffer, "F_SETBSDLKW");
			break;
		    case F_DIOINFO:
			strcat(buffer, "F_DIOINFO");
			break;
		    case F_RSETLK:
			strcat(buffer, "F_RSETLK");
			break;
		    case F_RGETLK:
			strcat(buffer, "F_RGETLK");
			break;
		    case F_RSETLKW:
			strcat(buffer, "F_RSETLKW");
			break;
		    case F_GETOWN:
			strcat(buffer, "F_GETOWN");
			break;
		    case F_SETOWN:
			strcat(buffer, "F_SETOWN");
			break;
		    default:
			sprintf(number, "%d", minor);
			strcat(buffer, number);
			break;
		}
		strcat( buffer, ")" );
		break;

	    case SYS_ulimit:
		strcpy( buffer, sysname[major] );
		strcat( buffer, "(" );
		switch (minor) {
		    case UL_GFILLIM:
			strcat(buffer, "get file size limit");
			break;
		    case UL_SFILLIM:
			strcat(buffer, "set file size limit");
			break;
		    case UL_GMEMLIM:
			strcat(buffer, "get break limit");
			break;
		    case UL_GDESLIM:
			strcat(buffer, "get open file limit");
			break;
		    case UL_GTXTOFF:
			strcat(buffer, "get text offset");
			break;
		    default:
			sprintf(number, "%d", minor);
			strcat(buffer, number);
			break;
		}
		strcat( buffer, ")" );
		break;

	    default:
		if (major == SAT_SYSCALL_KERNEL)
			strcpy( buffer, "Kernel action" );
		else if (major < sizeof(sysname)/sizeof(sysname[0]))
			strcpy( buffer, sysname[major] );
		else
			sprintf(buffer, "syscall #%d\n", major);
		break;
	}

	return buffer;
}
