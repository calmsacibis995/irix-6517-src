#ifndef __SYS_S__
#define __SYS_S__
#ifdef __cplusplus
extern "C" {
#endif
/*
*
* Copyright 1992, Silicon Graphics, Inc.
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
/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

#ident	"$Revision: 1.76 $"

/* System call numbers */
/* These must match with the table entries appearing in os/sysent.c */
#define SYSVoffset	1000  /* to offset us from 4.3BSD calls, when desired */

#define SYS_syscall	(0+SYSVoffset)
#define SYS_exit	(1+SYSVoffset)
#define SYS_fork	(2+SYSVoffset)
#define SYS_read	(3+SYSVoffset)
#define SYS_write	(4+SYSVoffset)
#define SYS_open	(5+SYSVoffset)
#define SYS_close	(6+SYSVoffset)
#define SYS_creat	(8+SYSVoffset)
#define SYS_link	(9+SYSVoffset)
#define SYS_unlink	(10+SYSVoffset)
#define SYS_execv	(11+SYSVoffset)
#define SYS_chdir	(12+SYSVoffset)
#define SYS_time	(13+SYSVoffset)
#define SYS_chmod	(15+SYSVoffset)
#define SYS_chown	(16+SYSVoffset)
#define SYS_brk		(17+SYSVoffset)
#define SYS_stat	(18+SYSVoffset)
#define SYS_lseek	(19+SYSVoffset)
#define SYS_getpid	(20+SYSVoffset)
#define SYS_mount	(21+SYSVoffset)
#define SYS_umount	(22+SYSVoffset)
#define SYS_setuid	(23+SYSVoffset)
#define SYS_getuid	(24+SYSVoffset)
#define SYS_stime	(25+SYSVoffset)
#define SYS_ptrace	(26+SYSVoffset)
#define SYS_alarm	(27+SYSVoffset)
#define SYS_pause	(29+SYSVoffset)
#define SYS_utime	(30+SYSVoffset)
#define SYS_access	(33+SYSVoffset)
#define SYS_nice	(34+SYSVoffset)
#define SYS_statfs	(35+SYSVoffset)
#define SYS_sync	(36+SYSVoffset)
#define SYS_kill	(37+SYSVoffset)
#define SYS_fstatfs	(38+SYSVoffset)
#define SYS_setpgrp	(39+SYSVoffset)
#define	SYS_syssgi	(40+SYSVoffset)
#define SYS_dup		(41+SYSVoffset)
#define SYS_pipe	(42+SYSVoffset)
#define SYS_times	(43+SYSVoffset)
#define SYS_profil	(44+SYSVoffset)
#define SYS_plock	(45+SYSVoffset)
#define SYS_setgid	(46+SYSVoffset)
#define SYS_getgid	(47+SYSVoffset)
#define SYS_msgsys	(49+SYSVoffset)
#define SYS_sysmips	(50+SYSVoffset)
#define SYS_acct	(51+SYSVoffset)
#define SYS_shmsys	(52+SYSVoffset)
#define SYS_semsys	(53+SYSVoffset)
#define SYS_ioctl	(54+SYSVoffset)
#define SYS_uadmin	(55+SYSVoffset)
#define SYS_sysmp	(56+SYSVoffset)
#define SYS_utssys	(57+SYSVoffset)
#define SYS_execve	(59+SYSVoffset)
#define SYS_umask	(60+SYSVoffset)
#define SYS_chroot	(61+SYSVoffset)
#define SYS_fcntl	(62+SYSVoffset)
#define SYS_ulimit	(63+SYSVoffset)
#define	SYS_getrlimit64	(75+SYSVoffset)
#define	SYS_setrlimit64	(76+SYSVoffset)
#define	SYS_nanosleep	(77+SYSVoffset)
#define	SYS_lseek64	(78+SYSVoffset)
#define SYS_rmdir	(79+SYSVoffset)
#define SYS_mkdir	(80+SYSVoffset)
#define SYS_getdents	(81+SYSVoffset)
#define	SYS_sginap	(82+SYSVoffset)
#define	SYS_sgikopt	(83+SYSVoffset)
#define SYS_sysfs	(84+SYSVoffset)
#define SYS_getmsg	(85+SYSVoffset)
#define SYS_putmsg	(86+SYSVoffset)
#define SYS_poll	(87+SYSVoffset)
/* This system call is internal for mips signal handling code */
#define SYS_sigreturn	(88+SYSVoffset)

/* 4.3BSD-compatible socket/protocol system calls */
#define SYS_accept	(89+SYSVoffset)
#define SYS_bind	(90+SYSVoffset)
#define SYS_connect	(91+SYSVoffset)
#define SYS_gethostid	(92+SYSVoffset)
#define SYS_getpeername	(93+SYSVoffset)
#define SYS_getsockname	(94+SYSVoffset)
#define SYS_getsockopt	(95+SYSVoffset)
#define SYS_listen	(96+SYSVoffset)
#define SYS_recv	(97+SYSVoffset)
#define SYS_recvfrom	(98+SYSVoffset)
#define SYS_recvmsg	(99+SYSVoffset)
#define SYS_select	(100+SYSVoffset)
#define SYS_send	(101+SYSVoffset)
#define SYS_sendmsg	(102+SYSVoffset)
#define SYS_sendto	(103+SYSVoffset)
#define SYS_sethostid	(104+SYSVoffset)
#define SYS_setsockopt	(105+SYSVoffset)
#define SYS_shutdown	(106+SYSVoffset)
#define SYS_socket	(107+SYSVoffset)
#define SYS_gethostname	(108+SYSVoffset)
#define SYS_sethostname	(109+SYSVoffset)
#define SYS_getdomainname (110+SYSVoffset)
#define SYS_setdomainname (111+SYSVoffset)

/* other 4.3BSD system calls */
#define SYS_truncate	(112+SYSVoffset)
#define SYS_ftruncate	(113+SYSVoffset)
#define SYS_rename	(114+SYSVoffset)
#define	SYS_symlink	(115+SYSVoffset)
#define	SYS_readlink	(116+SYSVoffset)
#define	SYS_nfssvc	(119+SYSVoffset)
#define	SYS_getfh	(120+SYSVoffset)
#define	SYS_async_daemon (121+SYSVoffset)
#define	SYS_exportfs	(122+SYSVoffset)
#define SYS_setregid	(123+SYSVoffset)
#define SYS_setreuid	(124+SYSVoffset)
#define SYS_getitimer	(125+SYSVoffset)
#define SYS_setitimer	(126+SYSVoffset)
#define	SYS_adjtime	(127+SYSVoffset)
#define	SYS_BSD_getime	(128+SYSVoffset)

/* Parallel processing system calls */
#define	SYS_sproc	(129+SYSVoffset)
#define	SYS_prctl	(130+SYSVoffset)
#define	SYS_procblk	(131+SYSVoffset)
#define	SYS_sprocsp	(132+SYSVoffset)

/* memory-mapped file calls */
#define	SYS_mmap	(134+SYSVoffset)
#define	SYS_munmap	(135+SYSVoffset)
#define	SYS_mprotect	(136+SYSVoffset)
#define	SYS_msync	(137+SYSVoffset)
#define	SYS_madvise	(138+SYSVoffset)
#define	SYS_pagelock	(139+SYSVoffset)
#define	SYS_getpagesize	(140+SYSVoffset)

/* BSD quotactl syscall, used to be libattach */
#define SYS_quotactl	(141+SYSVoffset)

/* 4.3BSD set/getpgrp syscalls */
#define SYS_BSDgetpgrp	(143+SYSVoffset)
#define SYS_BSDsetpgrp	(144+SYSVoffset)

/* 4.3BSD vhangup, fsync, and fchdir */ 
#define SYS_vhangup	(145+SYSVoffset)
#define SYS_fsync	(146+SYSVoffset)
#define SYS_fchdir	(147+SYSVoffset)

/* 4.3BSD getrlimit/setrlimit syscalls */
#define SYS_getrlimit	(148+SYSVoffset)
#define SYS_setrlimit	(149+SYSVoffset)

/* mips cache syscalls */
#define SYS_cacheflush	(150+SYSVoffset)
#define SYS_cachectl	(151+SYSVoffset)

/* 4.3BSD fchown/fchmod syscalls */
#define SYS_fchown	(152+SYSVoffset)
#define SYS_fchmod	(153+SYSVoffset)

/* 4.3BSD socketpair syscall */
#define SYS_socketpair	(155+SYSVoffset)

/* SVR4.1 new syscalls */
#define SYS_sysinfo	(156+SYSVoffset)
#define SYS_nuname	(157+SYSVoffset)
#define SYS_xstat	(158+SYSVoffset)
#define SYS_lxstat	(159+SYSVoffset)
#define SYS_fxstat	(160+SYSVoffset)
#define SYS_xmknod	(161+SYSVoffset)

/* Posix.1 signal calls */
#define SYS_ksigaction	(162+SYSVoffset)
#define SYS_sigpending	(163+SYSVoffset)
#define SYS_ksigprocmask (164+SYSVoffset)
#define SYS_sigsuspend	(165+SYSVoffset)

/* Posix.4 signal calls */
#define SYS_sigpoll	(166+SYSVoffset)
#define SYS_swapctl	(167+SYSVoffset)

#define SYS_getcontext	(168+SYSVoffset)
#define SYS_setcontext	(169+SYSVoffset)
#define SYS_waitsys	(170+SYSVoffset)
#define SYS_sigstack	(171+SYSVoffset)
#define SYS_sigaltstack	(172+SYSVoffset)
#define SYS_sigsendset	(173+SYSVoffset)

#define SYS_statvfs	(174+SYSVoffset)
#define SYS_fstatvfs	(175+SYSVoffset)
#define SYS_getpmsg	(176+SYSVoffset)
#define SYS_putpmsg	(177+SYSVoffset)

/* more SVR4.1 new syscall */
#define SYS_lchown	(178+SYSVoffset)
#define SYS_priocntl	(179+SYSVoffset)

/* Posix.4 signal calls */
#define SYS_ksigqueue	(180+SYSVoffset)

/* Newly lifted up  readv, writev */
#define SYS_readv	(181+SYSVoffset)
#define SYS_writev	(182+SYSVoffset)

/* xFS, 64-bit support */
#define	SYS_truncate64	(183+SYSVoffset)
#define	SYS_ftruncate64	(184+SYSVoffset)
#define	SYS_mmap64	(185+SYSVoffset)
#define	SYS_dmi		(186+SYSVoffset)
/* SVR4.2 ES-MP syscall */
#define	SYS_pread	(187+SYSVoffset)
#define	SYS_pwrite	(188+SYSVoffset)
	/* Posix.4 sync I/O calls */
#define	SYS_fdatasync	(189+SYSVoffset)
/* special SGI internal syscall */
#define	SYS_sgifastpath (190+SYSVoffset)        

/* attribute operations */
#define	SYS_attr_get	(191+SYSVoffset)        
#define	SYS_attr_getf	(192+SYSVoffset)        
#define	SYS_attr_set	(193+SYSVoffset)        
#define	SYS_attr_setf	(194+SYSVoffset)        
#define	SYS_attr_remove	(195+SYSVoffset)        
#define	SYS_attr_removef (196+SYSVoffset)        
#define	SYS_attr_list	(197+SYSVoffset)        
#define	SYS_attr_listf	(198+SYSVoffset)        
#define	SYS_attr_multi	(199+SYSVoffset)        
#define	SYS_attr_multif	(200+SYSVoffset)        

#define	SYS_statvfs64	(201+SYSVoffset)
#define	SYS_fstatvfs64	(202+SYSVoffset)

#define	SYS_getmountid	(203+SYSVoffset)

/* new sproc */
#define	SYS_nsproc	(204+SYSVoffset)

#define	SYS_getdents64	(205+SYSVoffset)
#define	SYS_afs_syscall	(206+SYSVoffset)
#define	SYS_ngetdents	(207+SYSVoffset)
#define	SYS_ngetdents64	(208+SYSVoffset)

/* Trusted Irix */
#define SYS_sgi_sesmgr	(209+SYSVoffset)
	
/* checkpoint/restart pidsprocsp */
#define	SYS_pidsprocsp	(210+SYSVoffset)
/* remote exec */
#define SYS_rexec		(211+SYSVoffset)
/* POSIX timer calls */
#define	SYS_timer_create	(212+SYSVoffset)
#define	SYS_timer_delete	(213+SYSVoffset)
#define	SYS_timer_settime	(214+SYSVoffset)
#define	SYS_timer_gettime	(215+SYSVoffset)
#define	SYS_timer_getoverrun	(216+SYSVoffset)
/* POSIX scheduler calls */	
#define	SYS_sched_rr_get_interval	(217+SYSVoffset)
#define	SYS_sched_yield		(218+SYSVoffset)
#define	SYS_sched_getscheduler	(219+SYSVoffset)
#define	SYS_sched_setscheduler	(220+SYSVoffset)
#define	SYS_sched_getparam	(221+SYSVoffset)
#define	SYS_sched_setparam	(222+SYSVoffset)
#define	SYS_usync_cntl		(223+SYSVoffset)
#define	SYS_psema_cntl		(224+SYSVoffset)
/* checkpoint/restart return from restart */
#define	SYS_restartreturn	(225+SYSVoffset)
#define SYS_sysget		(226+SYSVoffset)
/* XPG4 socket calls */
#define SYS_xpg4_recvmsg	(227+SYSVoffset)
/* UserMode Filesystem call */
#define SYS_umfscall		(228+SYSVoffset)
/* nsproc for checkpoint */
#define	SYS_nsproctid		(229+SYSVoffset)
/* Remote exec complete - Cellular IRIX, kernel-internal only */
#define SYS_rexec_complete	(230+SYSVoffset)
#define SYS_xpg4_sigaltstack	(231+SYSVoffset)
#define SYS_xpg4_select		(232+SYSVoffset)
#define SYS_xpg4_setregid	(233+SYSVoffset)
#define	SYS_linkfollow		(234+SYSVoffset)
#ifdef __cplusplus
}
#endif
#endif /* !__SYS_S__ */
