/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */

#ident "$Revision: 1.10 $"
#ifdef __STDC__
	#pragma weak sys_siglist = _sys_siglist
	#pragma weak sys_nsig = _sys_nsig
#endif

#include "synonyms.h"
#include <signal.h>

int _sys_nsig = NSIG;

/*
 * SVR4 ABI signal list
 */
char	*_sys_siglist[NSIG] = {
	"Signal 0",
	"Hangup",			/* SIGHUP */
	"Interrupt",			/* SIGINT */
	"Quit",				/* SIGQUIT */
	"Illegal instruction",		/* SIGILL */
	"Trace/BPT trap",		/* SIGTRAP */
	"Abort",			/* SIGABRT */
	"EMT trap",			/* SIGEMT */
	"Floating point exception",	/* SIGFPE */
	"Killed",			/* SIGKILL */
	"Bus error",			/* SIGBUS */
	"Segmentation fault",		/* SIGSEGV */
	"Bad system call",		/* SIGSYS */
	"Broken pipe",			/* SIGPIPE */
	"Alarm clock",			/* SIGALRM */
	"Terminated",			/* SIGTERM */
	"User defined signal 1",	/* SIGUSR1 */
	"User defined signal 2",	/* SIGUSR2 */
	"Child exited",			/* SIGCLD */
	"Power-fail restart",		/* SIGPWR */
	"Window size changes",		/* SIGWINCH */
	"Urgent I/O condition",		/* SIGURG */
	"Pollable event occured",	/* SIGPOLL/SIGIO */
	"Stop signal (not from tty)",	/* SIGSTOP */
	"Stop signal from tty",		/* SIGTSTP */
	"Continued",			/* SIGCONT */
	"Stopped (tty input)",		/* SIGTTIN */
	"Stopped (tty output)",		/* SIGTTOU */
	"Virtual time alarm",		/* SIGVTALRM */
	"Profiling alarm",		/* SIGPROF */
	"Cputime limit exceeded",	/* SIGXCPU */
	"Filesize limit exceeded",	/* SIGXFSZ */
	"Signal 32",
	"System reserved signal 33",	/* 33 */
	"System reserved signal 34",	/* 34 */
	"System reserved signal 35",	/* 35 */
	"System reserved signal 36",	/* 36 */
	"System reserved signal 37",	/* 37 */
	"System reserved signal 38",	/* 38 */
	"System reserved signal 39",	/* 39 */
	"System reserved signal 40",	/* 40 */
	"System reserved signal 41",	/* 41 */
	"System reserved signal 42",	/* 42 */
	"System reserved signal 43",	/* 43 */
	"System reserved signal 44",	/* 44 */
	"System reserved signal 45",	/* 45 */
	"System reserved signal 46",	/* 46 */
	"System reserved signal 47",	/* 47 */
	"System reserved signal 48",	/* 48 */
	"Real-time signal RTMIN",	/* SIGRTMIN */
	"Real-time signal RTMIN+1",	/* SIGRTMIN+1 */
	"Real-time signal RTMIN+2",	/* SIGRTMIN+2 */
	"Real-time signal RTMIN+3",	/* SIGRTMIN+3 */
	"Real-time signal RTMIN+4",	/* SIGRTMIN+4 */
	"Real-time signal RTMIN+5",	/* SIGRTMIN+5 */
	"Real-time signal RTMIN+6",	/* SIGRTMIN+6 */
	"Real-time signal RTMIN+7",	/* SIGRTMIN+7 */
	"Real-time signal RTMIN+8",	/* SIGRTMIN+8 */
	"Real-time signal RTMIN+9",	/* SIGRTMIN+9 */
	"Real-time signal RTMIN+10",	/* SIGRTMIN+10 */
	"Real-time signal RTMIN+11",	/* SIGRTMIN+11 */
	"Real-time signal RTMIN+12",	/* SIGRTMIN+12 */
	"Real-time signal RTMIN+13",	/* SIGRTMIN+13 */
	"Real-time signal RTMIN+14",	/* SIGRTMIN+14 */
	"Real-time signal RTMAX",	/* SIGRTMAX */
};

char *_sys_signames[NSIG] = {
		0,
/*  1 */	"HUP",
/*  2 */	"INT",
/*  3 */	"QUIT",
/*  4 */	"ILL",
/*  5 */	"TRAP",
/*  6 */	"ABRT",
/*  7 */	"EMT",
/*  8 */	"FPE",
/*  9 */	"KILL",
/* 10 */	"BUS",
/* 11 */	"SEGV",
/* 12 */	"SYS",
/* 13 */	"PIPE",
/* 14 */	"ALRM",
/* 15 */	"TERM",
/* 16 */	"USR1",
/* 17 */	"USR2",
/* 18 */	"CLD",
/* 19 */	"PWR",
/* 20 */	"WINCH",
/* 21 */	"URG",
/* 22 */	"POLL",
/* 23 */	"STOP",
/* 24 */	"TSTP",
/* 25 */	"CONT",
/* 26 */	"TTIN",
/* 27 */	"TTOU",
/* 28 */	"VTALRM",
/* 29 */	"PROF",
/* 30 */	"XCPU",
/* 31 */	"XFSZ",
/* 32 */	"Reserved32",
/* 33 */	"Reserved33",
/* 34 */	"Reserved34",
/* 35 */	"Reserved35",
/* 36 */	"Reserved36",
/* 37 */	"Reserved37",
/* 38 */	"Reserved38",
/* 39 */	"Reserved39",
/* 40 */	"Reserved40",
/* 41 */	"Reserved41",
/* 42 */	"Reserved42",
/* 43 */	"Reserved43",
/* 44 */	"Reserved44",
/* 45 */	"Reserved45",
/* 46 */	"Reserved46",
/* 47 */	"Reserved47",
/* 48 */	"Reserved48",
/* 49 */	"RTMIN",
/* 50 */	"RTMIN+1",
/* 51 */	"RTMIN+2",
/* 52 */	"RTMIN+3",
/* 53 */	"RTMIN+4",
/* 54 */	"RTMIN+5",
/* 55 */	"RTMIN+6",
/* 56 */	"RTMIN+7",
/* 57 */	"RTMIN+8",
/* 58 */	"RTMIN+9",
/* 59 */	"RTMIN+10",
/* 60 */	"RTMIN+11",
/* 61 */	"RTMIN+12",
/* 62 */	"RTMIN+13",
/* 63 */	"RTMIN+14",
/* 64 */	"RTMAX"
};
