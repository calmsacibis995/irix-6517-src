/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.2 $"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

#include "sh.h"
#include "sh.dir.h"
#include "sh.proc.h"
#include "sh.wconst.h"

/*
 * C shell
 */

#define	INF	1000

struct	biltins bfunc[] = {
	S_AT,		(bf_t)dolet,		0,	INF,
	S_alias,	(bf_t)doalias,		0,	INF,
#ifdef OLDMALLOC
	S_alloc,	(bf_t)showall,		0,	1,
#endif
	S_bg,		(bf_t)dobg,		0,	INF,
	S_break,	(bf_t)dobreak,		0,	0,
	S_breaksw,	(bf_t)doswbrk,		0,	0,
	S_case,		(bf_t)dozip,		0,	1,
	S_cd,		(bf_t)dochngd,		0,	1,
	S_chdir,	(bf_t)dochngd,		0,	1,
	S_continue,	(bf_t)docontin,		0,	0,
	S_default,	(bf_t)dozip,		0,	0,
	S_dirs,		(bf_t)dodirs,		0,	1,
	S_echo,		(bf_t)doecho,		0,	INF,
	S_else,		(bf_t)doelse,		0,	INF,
	S_end,		(bf_t)doend,		0,	0,
	S_endif,	(bf_t)dozip,		0,	0,
	S_endsw,	(bf_t)dozip,		0,	0,
	S_eval,		(bf_t)doeval,		0,	INF,
	S_exec,		(bf_t)execash,		1,	INF,
	S_exit,		(bf_t)doexit,		0,	INF,
	S_fg,		(bf_t)dofg,		0,	INF,
	S_foreach,	(bf_t)doforeach,	3,	INF,
	S_glob,		(bf_t)doglob,		0,	INF,
	S_goto,		(bf_t)dogoto,		1,	1,
	S_history,	(bf_t)dohist,		0,	2,
	S_if,		(bf_t)doif,		1,	INF,
	S_jobs,		(bf_t)dojobs,		0,	1,
	S_kill,		(bf_t)dokill,		1,	INF,
	S_limit,	(bf_t)dolimit,		0,	3,
	S_login,	(bf_t)dologin,		0,	1,
	S_logout,	(bf_t)dologout,		0,	0,
#ifdef NEWGRP
	S_newgrp,	(bf_t)donewgrp,		1,	1,
#endif
	S_nice,		(bf_t)donice,		0,	INF,
	S_nohup,	(bf_t)donohup,		0,	INF,
	S_notify,	(bf_t)donotify,		0,	INF,
	S_onintr,	(bf_t)doonintr,		0,	2,
	S_popd,		(bf_t)dopopd,		0,	1,
	S_pushd,	(bf_t)dopushd,		0,	1,
	S_rehash,	(bf_t)dohash,		0,	0,
	S_repeat,	(bf_t)dorepeat,		2,	INF,
	S_set,		(bf_t)doset,		0,	INF,
	S_setenv,	(bf_t)dosetenv,		0,	2,
	S_shift,	(bf_t)shift,		0,	1,
	S_source,	(bf_t)dosource,		1,	2,
	S_stop,		(bf_t)dostop,		1,	INF,
	S_suspend,	(bf_t)dosuspend,	0,	0,
	S_switch,	(bf_t)doswitch,		1,	INF,
	S_time,		(bf_t)dotime,		0,	INF,
	S_umask,	(bf_t)doumask,		0,	1,
	S_unalias,	(bf_t)unalias,		1,	INF,
	S_unhash,	(bf_t)dounhash,		0,	0,
	S_unlimit,	(bf_t)dounlimit,	0,	INF,
	S_unset,	(bf_t)unset,		1,	INF,
	S_unsetenv,	(bf_t)dounsetenv,	1,	INF,
	S_wait,		(bf_t)dowait,		0,	0,
	S_while,	(bf_t)dowhile,		1,	INF,
};

int nbfunc = sizeof bfunc / sizeof *bfunc;

#define	ZBREAK		0
#define	ZBRKSW		1
#define	ZCASE		2
#define	ZDEFAULT 	3
#define	ZELSE		4
#define	ZEND		5
#define	ZENDIF		6
#define	ZENDSW		7
#define	ZEXIT		8
#define	ZFOREACH	9
#define	ZGOTO		10
#define	ZIF		11
#define	ZLABEL		12
#define	ZLET		13
#define	ZSET		14
#define	ZSWITCH		15
#define	ZTEST		16
#define	ZTHEN		17
#define	ZWHILE		18

struct srch srchn[] = {
	S_AT,		ZLET,
	S_break,	ZBREAK,
	S_breaksw,	ZBRKSW,
	S_case,		ZCASE,
	S_default, 	ZDEFAULT,
	S_else,		ZELSE,
	S_end,		ZEND,
	S_endif,	ZENDIF,
	S_endsw,	ZENDSW,
	S_exit,		ZEXIT,
	S_foreach, 	ZFOREACH,
	S_goto,		ZGOTO,
	S_if,		ZIF,
	S_label,	ZLABEL,
	S_set,		ZSET,
	S_switch,	ZSWITCH,
	S_while,	ZWHILE
};
int nsrchn = sizeof srchn / sizeof *srchn;

struct	mesg mesg[NSIG+1] = {
	0,	0,		0,
	S_HUP,	_SGI_DMMX_csh_S_HUP,	"Hangup",			/* 1 */
	S_INT,	_SGI_DMMX_csh_S_INT,	"Interrupt",			/* 2 */	
	S_QUIT,	_SGI_DMMX_csh_S_QUIT,	"Quit",				/* 3 */
	S_ILL,	_SGI_DMMX_csh_S_ILL,	"Illegal instruction",		/* 4 */
	S_TRAP,	_SGI_DMMX_csh_S_TRAP,	"Trace/BPT trap",		/* 5 */
	S_ABRT,	_SGI_DMMX_csh_S_ABRT,	"Abort",			/* 6 */
	S_EMT,	_SGI_DMMX_csh_S_EMT,	"Emulator trap",		/* 7 */
	S_FPE,	_SGI_DMMX_csh_S_FPE,	"Arithmetic exception",		/* 8 */
	S_KILL,	_SGI_DMMX_csh_S_KILL,	"Killed",			/* 9 */
	S_BUS,	_SGI_DMMX_csh_S_BUS,	"Bus error",			/* 10 */
	S_SEGV,	_SGI_DMMX_csh_S_SEGV,	"Segmentation fault",		/* 11 */
	S_SYS,	_SGI_DMMX_csh_S_SYS,	"Bad system call",		/* 12 */
	S_PIPE,	_SGI_DMMX_csh_S_PIPE,	"Broken pipe",			/* 13 */
	S_ALRM,	_SGI_DMMX_csh_S_ALRM,	"Alarm clock",			/* 14 */
	S_TERM,	_SGI_DMMX_csh_S_TERM,	"Terminated",			/* 15 */
	S_USR1,	_SGI_DMMX_csh_S_USR1,	"User defined signal 1",	/* 16 */
	S_USR2,	_SGI_DMMX_csh_S_USR2,	"User defined signal 2",	/* 17 */
	S_CHLD,	_SGI_DMMX_csh_S_CHLD,	"Child exited",			/* 18 */
	S_PWR,	_SGI_DMMX_csh_S_PWR,	"Power failed",			/* 19 */
	S_WINCH,_SGI_DMMX_csh_S_WINCH,	"Window size changed",		/* 20 */
	S_URG,	_SGI_DMMX_csh_S_URG,	"Urgent I/O condition",		/* 21 */
	S_POLL,	_SGI_DMMX_csh_S_POLL,	"Pollable event occurred",	/* 22 */
	S_STOP,	_SGI_DMMX_csh_S_STOP,	"Stopped (signal)",		/* 23 */
	S_TSTP,	_SGI_DMMX_csh_S_TSTP,	"Stopped",			/* 24 */
	S_CONT,	_SGI_DMMX_csh_S_CONT,	"Continued",			/* 25 */
	S_TTIN, _SGI_DMMX_csh_S_TTIN,	"Stopped (tty input)",		/* 26 */
	S_TTOU, _SGI_DMMX_csh_S_TTOU,	"Stopped (tty output)",		/* 27 */
	S_VTALRM,_SGI_DMMX_csh_S_VTALRM,"Virtual timer expired",	/* 28 */
	S_PROF,	_SGI_DMMX_csh_S_PROF,	"Profiling timer expired",	/* 29 */
	S_XCPU,	_SGI_DMMX_csh_S_XCPU,	"Cputime limit exceeded",	/* 30 */
	S_XFSZ, _SGI_DMMX_csh_S_XFSZ,	"Filesize limit exceeded",	/* 31 */
	0,	_SGI_DMMX_csh_S_32,	"Signal 32"			/* 32 */
};
