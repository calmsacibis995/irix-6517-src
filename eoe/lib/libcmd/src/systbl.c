/*
 * systbl.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.2 $"


/* These routines map system call number <=> system call name*/

#include	<string.h>
#include	<sys/types.h>
#include	<sys/syscall.h>

extern	char	*strcpy();
extern	int	strcmp();

#ifdef NOTDEF
struct	sysdef {
	const char	*name;
	int	number;
};
const static	struct	sysdef	sdefs[] = {
	{ "exit",		SYS_exit},
	{ "fork",		SYS_fork},
	{ "read",		SYS_read},
	{ "write",		SYS_write},
	{ "open",		SYS_open},
	{ "close",		SYS_close},
	{ "wait",		SYS_wait},
	{ "creat",		SYS_creat},
	{ "link",		SYS_link},
	{ "unlink",		SYS_unlink},
	{ "exec",		SYS_exec},
	{ "chdir",		SYS_chdir},
	{ "time",		SYS_time},
	{ "mknod",		SYS_mknod},
	{ "chmod",		SYS_chmod},
	{ "chown",		SYS_chown},
	{ "brk",		SYS_brk	},
	{ "stat",		SYS_stat},
	{ "lseek",		SYS_lseek},
	{ "getpid",		SYS_getpid},
	{ "mount",		SYS_mount},
	{ "umount",		SYS_umount},
	{ "setuid",		SYS_setuid},
	{ "getuid",		SYS_getuid},
	{ "stime",		SYS_stime},
	{ "ptrace",		SYS_ptrace},
	{ "alarm",		SYS_alarm},
	{ "fstat",		SYS_fstat},
	{ "pause",		SYS_pause},
	{ "utime",		SYS_utime},
	{ "stty",		SYS_stty},
	{ "gtty",		SYS_gtty},
	{ "access",		SYS_access},
	{ "nice",		SYS_nice},
	{ "statfs",		SYS_statfs},
	{ "sync",		SYS_sync},
	{ "kill",		SYS_kill},
	{ "fstatfs",		SYS_fstatfs},
	{ "pgrpsys",		SYS_pgrpsys},
	{ "xenix",		SYS_xenix},
	{ "dup",		SYS_dup},
	{ "pipe",		SYS_pipe},
	{ "times",		SYS_times},
	{ "profil",		SYS_profil},
	{ "plock",		SYS_plock},
	{ "setgid",		SYS_setgid},
	{ "getgid",		SYS_getgid},
	{ "signal",		SYS_signal},
	{ "msgsys",		SYS_msgsys},
	{ "sys3b",		SYS_sys3b},
	{ "acct",		SYS_acct},
	{ "shmsys",		SYS_shmsys},
	{ "semsys",		SYS_semsys},
	{ "ioctl",		SYS_ioctl},
	{ "uadmin",		SYS_uadmin},
	{ "",			-1        }, /*56*/
	{ "utssys",		SYS_utssys},
	{ "fsync",		SYS_fsync},
	{ "execve",		SYS_execve},
	{ "umask",		SYS_umask},
	{ "chroot",		SYS_chroot},
	{ "fcntl",		SYS_fcntl},
	{ "ulimit",		SYS_ulimit},
	{ "",			-1        },   /*64-77*/
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "",			-1        },
	{ "rfsys",		SYS_rfsys},
	{ "rmdir",		SYS_rmdir},
	{ "mkdir",		SYS_mkdir},
	{ "getdents",		SYS_getdents},
	{ "",			-1        },  /*82-83*/
	{ "",			-1        },
	{ "sysfs",		SYS_sysfs},
	{ "getmsg",		SYS_getmsg},
	{ "putmsg",		SYS_putmsg},
	{ "poll",		SYS_poll},
	{ "lstat",		SYS_lstat},
	{ "symlink",		SYS_symlink},
	{ "readlink",		SYS_readlink},
	{ "setgroups",		SYS_setgroups},
	{ "getgroups",		SYS_getgroups},
	{ "fchmod",		SYS_fchmod},
	{ "fchown",		SYS_fchown},
	{ "sigprocmask"	,	SYS_sigprocmask},
	{ "sigsuspend",		SYS_sigsuspend},
	{ "sigaltstack"	,	SYS_sigaltstack},
	{ "sigaction",		SYS_sigaction},
	{ "sigpending",		SYS_sigpending},
	{ "context",		SYS_context},
	{ "evsys",		SYS_evsys},
	{ "evtrapret",		SYS_evtrapret},
	{ "statvfs",		SYS_statvfs},
	{ "fstatvfs",		SYS_fstatvfs},
	{ "",			-1        },
	{ "nfssys",		SYS_nfssys},
	{ "waitsys",		SYS_waitsys},
	{ "sigsendsys",		SYS_sigsendsys},
	{ "hrtsys",		SYS_hrtsys},
	{ "acancel",		SYS_acancel},
	{ "async",		SYS_async},
	{ "priocntlsys"	,	SYS_priocntlsys},
	{ "pathconf",		SYS_pathconf},
	{ "mincore",		SYS_mincore},
	{ "mmap",		SYS_mmap},
	{ "mprotect",		SYS_mprotect},
	{ "munmap",		SYS_munmap},
	{ "fpathconf",		SYS_fpathconf},
	{ "vfork",		SYS_vfork},
	{ "fchdir",		SYS_fchdir},
	{ "readv",		SYS_readv},
	{ "writev",		SYS_writev},
	{ "xstat",		SYS_xstat},
	{ "lxstat",		SYS_lxstat},
	{ "fxstat",		SYS_fxstat},
	{ "xmknod",		SYS_xmknod},
	{ "clocal",		SYS_clocal},
	{ "setrlimit",		SYS_setrlimit},
	{ "getrlimit",		SYS_getrlimit},
	{ "lchown",		SYS_lchown},
	{ "memcntl",		SYS_memcntl},
	{ "getpmsg",		SYS_getpmsg},
	{ "putpmsg",		SYS_putpmsg},
	{ "rename",		SYS_rename},
	{ "uname",		SYS_uname},
	{ "setegid",		SYS_setegid},
	{ "sysconfig",		SYS_sysconfig},
	{ "adjtime",		SYS_adjtime},
	{ "systeminfo",		SYS_systeminfo},
	{ "",			-1        },    /*140*/
	{ "seteuid",		SYS_seteuid},
	{ "",			-1        },    /*142*/
	{ "",			-1        },    /*143*/
	{ "secsys",		SYS_secsys},
	{ "filepriv",		SYS_filepriv},
	{ "procpriv",		SYS_procpriv},
	{ "devstat",		SYS_devstat},
	{ "aclipc",		SYS_aclipc},
	{ "fdevstat",		SYS_fdevstat},
	{ "flvlfile",		SYS_flvlfile},
	{ "lvlfile",		SYS_lvlfile},
	{ "",			-1        },    /*152*/
	{ "lvlequal",		SYS_lvlequal},
	{ "lvlproc",		SYS_lvlproc},
	{ "",			-1        },
	{ "lvlipc",		SYS_lvlipc},
	{ "acl",		SYS_acl},
	{ "auditbuf",		SYS_auditbuf},
	{ "auditctl",		SYS_auditctl},
	{ "auditdmp",		SYS_auditdmp},
	{ "auditevt",		SYS_auditevt},
	{ "auditlog",		SYS_auditlog},
	{ "lvldom",		SYS_lvldom},
	{ "lvlvfs",		SYS_lvlvfs},
	{ "mkmld",		SYS_mkmld},
	{ "mldmode",		SYS_mldmode},
	{ "secadvise",		SYS_secadvise},
};

#define	NUM_SYSCALL (sizeof(sdefs)/sizeof(struct sysdef))

/*Given the system call number return the system call name*/
/*If the system call number is not defined then a NULL pointer is returned*/ 
char	*
scallnam(buf, p)
char	*buf;
int	p;
{
	register int	i;

	for(i = 0; i < NUM_SYSCALL; ++i) {
		if (sdefs[i].number == p) {
			return strcpy(buf, sdefs[i].name);
		}
	}
	*buf='\0';
	return (char *)0;
}

/*Given the system call name return the system call number*/
/*If the system call name is not defined then a -1 is returned*/
int
scallnum(name)
char	*name;
{
	register int	i;

	if (!(*name))
		return -1;
	for(i = 0; i < NUM_SYSCALL; ++i){
		if (!strcmp(name, sdefs[i].name)) {
			return sdefs[i].number;
		}
	}
	return -1;
}

#endif /* NOTDEF */


