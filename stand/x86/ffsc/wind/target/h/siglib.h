/* sigLib.h - obsolete vxWorks 5.0 header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,09feb93,rrr  added SIGCONTEXT typedef for compatibility with 5.0.
01e,05feb93,rrr  fixed spr 1906 (signal numbers to match sun os)
01d,22sep92,rrr  added support for c++
01c,21sep92,rrr  more stuff to be more like 5.0.
01a,20sep92,smb  changed signalP.h to sigLibP.h
01a,19sep92,jcf  written for compatibility with 5.0.
*/

#ifndef __INCsigLibh
#define __INCsigLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "signal.h"
#include "sigcodes.h"
#include "unistd.h"
#include "private/siglibp.h"

#define NUM_SIGNALS	32

#define SIGIO		(SIGRTMIN + 0)
#define SIGXCPU		(SIGRTMIN + 1)
#define SIGXFSZ		(SIGRTMIN + 2)
#define SIGVTALRM	(SIGRTMIN + 3)
#define SIGPROF		(SIGRTMIN + 4)
#define SIGWINCH	(SIGRTMIN + 5)
#define SIGLOST		(SIGRTMIN + 6)
#define SIGURG		16

#define SIGIOT		SIGABRT
#define SIGCLD		SIGCHLD
#define SIGPOLL		SIGIO
#define SIGSYS		SIGFMT

#define NUM_SIGNALS	32

typedef struct sigvec SIGVEC;
typedef struct sigcontext SIGCONTEXT;

#ifdef __cplusplus
}
#endif

#endif /* __INCsigLibh */
