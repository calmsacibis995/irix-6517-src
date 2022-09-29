/* usrLib.h - header for user interface subroutines */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
02f,27may95,p_m  added spy routines prototypes.
02e,22sep92,rrr  added support for c++
02d,18sep92,smb  moved mkdir and rmdir prototype to sys/stat.h and
		 unistd.h repecatively and included sys/stat.h and unistd.h
02c,29jul92,smb  changed parameter to printErrno from errno to errNo.
02b,15jul92,jmm  changed ld() to return MODULE_ID rather than STATUS
02a,04jul92,jcf  cleaned up.
01v,25jun92,yao  added ANSI definition for pc().
01u,16jun92,yao  changed declaration for mRegs().
01t,26may92,rrr  the tree shuffle
01s,20jan92,yao  removed ANSI definition for register displaying routines.
		 removed conditional CPU_FAMILY != I960.  changed ANSI
		 propotype definition for d(), m() and mRegs().
01r,09jan92,jwt  converted CPU==SPARC to CPU_FAMILY==SPARC.
01q,07nov91,hdn  added defines F0 - F15, FPMCR, FPSR, FPQR for G200.
01p,07nov91,wmd  conditionalized declaration of d() with BYTE_ORDER.
01o,29oct91,shl  removed duplicate rename() prototype -- SPR #927.
01n,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01m,20aug91,ajm  added MIPS_R3k support.
01l,14aug91,del  (intel) added FPx regs for I960KB support.
01k,29apr91,hdn  added defines and macros for TRON architecture.
01j,03feb90,del  added I960 support.
01i,05oct90,dnw  deleted private functions.
		 changed spawn, etc, to take var args.
01h,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01g,20aug89,gae  changed ifdef to CPU_FAMILY.
01f,16jul88,ecs  added SPARC register codes.
01e,24dec86,gae  changed stsLib.h to vwModNum.h.
01d,20aug86,llk  added register codes.
01c,10feb86,dnw  deleted obsolete definition of S_usrLib_NO_FREE_TID.
01b,13aug84,dnw  changed name to usrLib.
01a,06aug84,ecs  written
*/

#ifndef __INCusrLibh
#define __INCusrLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "regs.h"
#include "fpplib.h"
#include "modulelib.h"
#include "sys/stat.h"

/* usrLib status codes */

#define S_usrLib_NOT_ENOUGH_ARGS	(M_usrLib | 1)


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void 	help (void);
extern void 	netHelp (void);
extern void 	bootChange (void);
extern void 	periodRun (int sec,FUNCPTR rtn,int,int,int,int,int,int,int,int);
extern int 	period (int secs, FUNCPTR func,int,int,int,int,int,int,int,int);
extern void 	repeatRun (int n, FUNCPTR func,int,int,int,int,int,int,int,int);
extern int 	repeat (int n, FUNCPTR func, int,int,int,int,int,int,int,int);
extern int 	sp (FUNCPTR func, int,int,int,int,int,int,int,int,int);
extern int 	taskIdFigure (int taskNameOrId);
extern void 	checkStack (int taskNameOrId);
extern void 	i (int taskNameOrId);
extern void 	ts (int taskNameOrId);
extern void 	tr (int taskNameOrId);
extern void 	td (int taskNameOrId);
extern void 	ti (int taskNameOrId);
extern void 	version (void);
extern void 	m (void *adrs, int width);
extern void 	d (void *adrs, int nwords, int width);
extern STATUS 	cd (char *name);
extern void 	pwd (void);
extern STATUS 	copy (char *in, char *out);
extern STATUS 	copyStreams (int inFd, int outFd);
extern STATUS 	diskFormat (char *devName);
extern STATUS 	diskInit (char *devName);
extern STATUS 	squeeze (char *devName);
extern MODULE_ID ld (int syms, BOOL noAbort, char *name);
extern STATUS 	ls (char *dirName, BOOL doLong);
extern STATUS 	ll (char *dirName);
extern STATUS 	lsOld (char *dirName);
extern STATUS 	rm (char *fileName);
extern void 	devs (void);
extern void 	lkup (char *substr);
extern void 	lkAddr (unsigned int addr);
extern STATUS 	mRegs (char *regName, int taskNameOrId);
extern void 	printErrno (int errNo);
extern void 	printLogo (void);
extern void 	logout (void);
extern void 	h (int size);
extern int 	pc (int task);
extern void 	show (int objId, int level);
extern STATUS	spyClkStart (int intsPerSec);
extern void	spyClkStop (void);
extern void	spy (int freq, int ticksPerSec);
extern void	spyStop (void);
extern void	spyHelp (void);
extern void	spyReport (void);
extern void	spyTask (int freq);

#else

extern void 	help ();
extern void 	netHelp ();
extern void 	bootChange ();
extern void 	periodRun ();
extern int 	period ();
extern void 	repeatRun ();
extern int 	repeat ();
extern int 	sp ();
extern int 	taskIdFigure ();
extern void 	checkStack ();
extern void 	i ();
extern void 	ts ();
extern void 	tr ();
extern void 	td ();
extern void 	ti ();
extern void 	version ();
extern void 	m ();
extern void 	d ();
extern STATUS 	cd ();
extern void 	pwd ();
extern STATUS 	copy ();
extern STATUS 	copyStreams ();
extern STATUS 	diskFormat ();
extern STATUS 	diskInit ();
extern STATUS 	squeeze ();
extern MODULE_ID ld ();
extern STATUS 	ls ();
extern STATUS 	ll ();
extern STATUS 	lsOld ();
extern STATUS 	rm ();
extern void 	devs ();
extern void 	lkup ();
extern void 	lkAddr ();
extern STATUS 	mRegs ();
extern void 	printErrno ();
extern void 	printLogo ();
extern void 	logout ();
extern void 	h ();
extern int 	pc ();
extern void 	show ();
extern STATUS	spyClkStart ();
extern void	spyClkStop ();
extern void	spy ();
extern void	spyStop ();
extern void	spyHelp ();
extern void	spyReport ();
extern void	spyTask ();

#endif	/* __STDC__ */


#ifdef __cplusplus
}
#endif

#endif /* __INCusrLibh */
