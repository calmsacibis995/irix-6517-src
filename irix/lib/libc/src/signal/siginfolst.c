/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/siginfolst.c	1.5"

#include "synonyms.h"
#include <signal.h>
#include <siginfo.h>


char * _sys_traplist[NSIGTRAP] = {
	"process breakpoint",
	"process trace"
};

char * _sys_illlist[NSIGILL] = {
	"illegal opcode",
	"illegal operand",
	"illegal addressing mode",
	"illegal trap",
	"privileged opcode",
	"privileged register",
	"co-processor",
	"bad stack"
};

char * _sys_fpelist[NSIGFPE] = {
	"integer divide by zero",
	"integer overflow",
	"floating point divide by zero",
	"floating point overflow",
	"floating point underflow",
	"floating point inexact result",
	"invalid floating point operation",
	"floating point subscript out of range"
};

char * _sys_segvlist[NSIGSEGV] = {
	"address not mapped to object",
	"invalid permissions"
};

char * _sys_buslist[NSIGBUS] = {
	"invalid address alignment",
	"non-existent physical address",
	"object specific"
};

char * _sys_cldlist[NSIGCLD] = {
	"child has exited",
	"child was killed",
	"child has coredumped",
	"traced child has trapped",
	"child has stopped",
	"stopped child has continued"
};

char * _sys_polllist[NSIGPOLL] = {
	"input available",
	"output possible",
	"message available",
	"I/O error",
	"high priority input available",
	"device disconnected"
};

struct siginfolist _sys_siginfolist[NSIG-1] = {
	0,		0,		/* SIGHUP */
	0,		0,		/* SIGINT */
	0,		0,		/* SIGQUIT */
	NSIGILL,	_sys_illlist,	/* SIGILL */
	NSIGTRAP,	_sys_traplist,	/* SIGTRAP */
	0,		0,		/* SIGABRT */
	0,		0,		/* SIGEMT */
	NSIGFPE,	_sys_fpelist,	/* SIGFPE */
	0,		0,		/* SIGKILL */
	NSIGBUS,	_sys_buslist,	/* SIGBUS */
	NSIGSEGV,	_sys_segvlist,	/* SIGSEGV */
	0,		0,		/* SIGSYS */
	0,		0,		/* SIGPIPE */
	0,		0,		/* SIGALRM */
	0,		0,		/* SIGTERM */
	0,		0,		/* SIGUSR1 */
	0,		0,		/* SIGUSR2 */
	NSIGCLD,	_sys_cldlist,	/* SIGCLD */
	0,		0,		/* SIGPWR */
	0,		0,		/* SIGWINCH */
	0,		0,		/* SIGURG */
	NSIGPOLL,	_sys_polllist,	/* SIGPOLL */
	0,		0,		/* SIGSTOP */
	0,		0,		/* SIGTSTP */
	0,		0,		/* SIGCONT */
	0,		0,		/* SIGTTIN */
	0,		0,		/* SIGTTOU */
	0,		0,		/* SIGVTALRM */
	0,		0,		/* SIGPROF */
	0,		0,		/* SIGXCPU */
	0,		0,		/* SIGXFSZ */
	0,		0,		/* SIG32 */
	0,		0,		/* SIGRESV33 */
	0,		0,		/* SIGRESV34 */
	0,		0,		/* SIGRESV35 */
	0,		0,		/* SIGRESV36 */
	0,		0,		/* SIGRESV37 */
	0,		0,		/* SIGRESV38 */
	0,		0,		/* SIGRESV39 */
	0,		0,		/* SIGRESV40 */
	0,		0,		/* SIGRESV41 */
	0,		0,		/* SIGRESV42 */
	0,		0,		/* SIGRESV43 */
	0,		0,		/* SIGRESV44 */
	0,		0,		/* SIGRESV45 */
	0,		0,		/* SIGRESV46 */
	0,		0,		/* SIGRESV47 */
	0,		0,		/* SIGRESV48 */
	0,		0,       	/* SIGRTMIN */
        0,		0,     		/* SIGRTMIN+1 */
        0,		0,     		/* SIGRTMIN+2 */
        0,		0,     		/* SIGRTMIN+3 */
        0,		0,     		/* SIGRTMIN+4 */
        0,		0,     		/* SIGRTMIN+5 */
        0,		0,     		/* SIGRTMIN+6 */
        0,		0,     		/* SIGRTMIN+7 */
        0,		0,     		/* SIGRTMIN+8 */
        0,		0,     		/* SIGRTMIN+9 */
        0,		0,    		/* SIGRTMIN+10 */
        0,		0,    		/* SIGRTMIN+11 */
        0,		0,    		/* SIGRTMIN+12 */
        0,		0,    		/* SIGRTMIN+13 */
        0,		0,    		/* SIGRTMIN+14 */
        0,		0,		/* SIGRTMAX */
};

#define MSG_ID_TRAP	853	    /* Offset of msg in uxsgicore catalog */
#define MSG_ID_ILL	MSG_ID_TRAP + NSIGTRAP
#define MSG_ID_FPE	MSG_ID_ILL + NSIGILL
#define MSG_ID_SEGV	MSG_ID_FPE + NSIGFPE
#define MSG_ID_BUS	MSG_ID_SEGV + NSIGSEGV
#define MSG_ID_CLD	MSG_ID_BUS + NSIGBUS
#define MSG_ID_POLL	MSG_ID_CLD + NSIGCLD

const int _siginfo_msg_offset[NSIG-1] = {
	0,		/* SIGHUP */
	0,		/* SIGINT */
	0,		/* SIGQUIT */
	MSG_ID_ILL,	/* SIGILL */
	MSG_ID_TRAP,	/* SIGTRAP */
	0,		/* SIGABRT */
	0,		/* SIGEMT */
	MSG_ID_FPE,	/* SIGFPE */
	0,		/* SIGKILL */
	MSG_ID_BUS,	/* SIGBUS */
	MSG_ID_SEGV,	/* SIGSEGV */
	0,		/* SIGSYS */
	0,		/* SIGPIPE */
	0,		/* SIGALRM */
	0,		/* SIGTERM */
	0,		/* SIGUSR1 */
	0,		/* SIGUSR2 */
	MSG_ID_CLD,	/* SIGCLD */
	0,		/* SIGPWR */
	0,		/* SIGWINCH */
	0,		/* SIGURG */
	MSG_ID_POLL,	/* SIGPOLL */
	0,		/* SIGSTOP */
	0,		/* SIGTSTP */
	0,		/* SIGCONT */
	0,		/* SIGTTIN */
	0,		/* SIGTTOU */
	0,		/* SIGVTALRM */
	0,		/* SIGPROF */
	0,		/* SIGXCPU */
	0,		/* SIGXFSZ */
	0,              /* SIG32 */
        0,              /* SIGRESV33 */
        0,              /* SIGRESV34 */
        0,              /* SIGRESV35 */
        0,              /* SIGRESV36 */
        0,              /* SIGRESV37 */
        0,              /* SIGRESV38 */
        0,              /* SIGRESV39 */
        0,              /* SIGRESV40 */
        0,              /* SIGRESV41 */
        0,              /* SIGRESV42 */
        0,              /* SIGRESV43 */
        0,              /* SIGRESV44 */
        0,              /* SIGRESV45 */
        0,              /* SIGRESV46 */
        0,              /* SIGRESV47 */
        0,              /* SIGRESV48 */
        0,              /* SIGRTMIN */
        0,              /* SIGRTMIN+1 */
        0,              /* SIGRTMIN+2 */
        0,              /* SIGRTMIN+3 */
        0,              /* SIGRTMIN+4 */
        0,              /* SIGRTMIN+5 */
        0,              /* SIGRTMIN+6 */
        0,              /* SIGRTMIN+7 */
        0,              /* SIGRTMIN+8 */
        0,              /* SIGRTMIN+9 */
        0,              /* SIGRTMIN+10 */
        0,              /* SIGRTMIN+11 */
        0,              /* SIGRTMIN+12 */
        0,              /* SIGRTMIN+13 */
        0,              /* SIGRTMIN+14 */
        0,              /* SIGRTMAX */
};
