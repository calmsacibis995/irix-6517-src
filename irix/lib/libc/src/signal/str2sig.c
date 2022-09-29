/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/str2sig.c	1.2"
#ifdef __STDC__
	#pragma weak str2sig = _str2sig
	#pragma weak sig2str = _sig2str
#endif
#include "synonyms.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static const struct signame {
	char	sigstr[11];
	unsigned char	signum;
} signames[] = {
	{ "EXIT",	0 },
	{ "HUP",	SIGHUP },
	{ "INT",	SIGINT },
	{ "QUIT",	SIGQUIT },
	{ "ILL",	SIGILL },
	{ "TRAP",	SIGTRAP },
	{ "ABRT",	SIGABRT },
	{ "IOT",	SIGIOT },
	{ "EMT",	SIGEMT },
	{ "FPE",	SIGFPE },
	{ "KILL",	SIGKILL },
	{ "BUS",	SIGBUS },
	{ "SEGV",	SIGSEGV },
	{ "SYS",	SIGSYS },
	{ "PIPE",	SIGPIPE },
	{ "ALRM",	SIGALRM },
	{ "TERM",	SIGTERM },
	{ "USR1",	SIGUSR1 },
	{ "USR2",	SIGUSR2 },
	{ "CLD",	SIGCLD },
	{ "CHLD",	SIGCHLD },
	{ "PWR",	SIGPWR },
	{ "WINCH",	SIGWINCH },
	{ "URG",	SIGURG },
	{ "POLL",	SIGPOLL },
	{ "IO",		SIGPOLL },
	{ "STOP",	SIGSTOP },
	{ "TSTP",	SIGTSTP },
	{ "CONT",	SIGCONT },
	{ "TTIN",	SIGTTIN },
	{ "TTOU",	SIGTTOU },
	{ "VTALRM",	SIGVTALRM },
	{ "PROF",	SIGPROF },
	{ "XCPU",	SIGXCPU },
	{ "XFSZ",	SIGXFSZ },
	{ "Reserved33", 33	},
	{ "Reserved34", 34 },
	{ "Reserved35", 35 },
	{ "Reserved36", 36 },
	{ "Reserved37", 37 },
	{ "Reserved38", 38 },
	{ "Reserved39", 39 },
	{ "Reserved40", 40 },
	{ "Reserved41", 41 },
	{ "Reserved42", 42 },
	{ "Reserved43", 43 },
	{ "Reserved44", 44 },
	{ "Reserved45", 45 },
	{ "Reserved46", 46 },
	{ "Reserved47", 47 },
	{ "Reserved48", 48 },
	{ "RTMIN", 	SIGRTMIN },
	{ "RTMIN+1", 	SIGRTMIN+1 },
	{ "RTMIN+2", 	SIGRTMIN+2 },
	{ "RTMIN+3", 	SIGRTMIN+3 },
	{ "RTMIN+4", 	SIGRTMIN+4 },
	{ "RTMIN+5", 	SIGRTMIN+5 },
	{ "RTMIN+6", 	SIGRTMIN+6 },
	{ "RTMIN+7", 	SIGRTMIN+7 },
	{ "RTMIN+8", 	SIGRTMIN+8 },
	{ "RTMIN+9", 	SIGRTMIN+9 },
	{ "RTMIN+10", 	SIGRTMIN+10 },
	{ "RTMIN+11", 	SIGRTMIN+11 },
	{ "RTMIN+12", 	SIGRTMIN+12 },
	{ "RTMIN+13", 	SIGRTMIN+13 },
	{ "RTMIN+14", 	SIGRTMIN+14 },
	{ "RTMAX", 	SIGRTMAX },
};

#define SIGCNT (sizeof(signames)/sizeof(struct signame))

int
str2sig(char *s, int *sigp)
{
	register const struct signame *sp;
	
	if (*s >= '0' && *s <= '9') {
		int i = atoi(s);
		for (sp = signames; sp < &signames[SIGCNT]; sp++) {
			if (sp->signum == i) {
				*sigp = sp->signum;
				return (0);
			}
		}
		return(-1);
	} else {
		for (sp = signames; sp < &signames[SIGCNT]; sp++) {
			if (strcmp(sp->sigstr,s) == 0) {
				*sigp = sp->signum;
				return (0);
			}
		}
		return(-1);
	}
}

int
sig2str(int i, char *s)
{
	register const struct signame *sp;
	
	for (sp = signames; sp < &signames[SIGCNT]; sp++) {
		if (sp->signum == i) {
			(void) strcpy(s,sp->sigstr);
			return (0);
		}
	}
	return (-1);
}
