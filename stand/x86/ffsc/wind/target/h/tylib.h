/* tyLib.h - tty handler support library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
03b,22sep92,rrr  added support for c++
03a,04jul92,jcf  cleaned up.
02u,26may92,rrr  the tree shuffle
02t,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
02s,11jul91,jwt  removed divisible bit-field operations in TY_DEV structure.
02r,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
02q,07aug90,shl  added function declarations comment.
02p,26jun90,jcf  embedded the TY_DEV semaphores.
02o,12may90,gae  added IMPORTs of ty{Read,Write,ITx,IRd}.
		 removed unused wdog.  changed UTINY to UINT8.
02n,29mar90,rdc  reworked to lower interrupt latency.
02m,16mar90,rdc  added select support.
02l,27jul89,hjb  added protoHook and protoArg to TY_DEV.
02k,18nov88,dnw  removed NOT_GENERIC stuff.
02j,04may88,jcf  changed SEMAPHORE to SEM_ID.
02i,15jun87,ecs  added canceled to rdState & wrtState of TY_DEV.
02h,24dec86,gae  changed stsLib.h to vwModNum.h.
02g,07apr86,dnw  removed ST_ERROR status and added rdError flag to smpte mode.
02f,23mar86,jlf  changed GENERIC to NOT_GENERIC.
02e,22aug85,dnw	 replaced xState with rdState and wrtState structures.
		 added rdSmpteState and wrtSmpteState to SMPTE only version.
02d,16aug84,dnw  removed S_tyLib_SMPTE_READ_ERROR from GENERIC version.
		 added lnNBytes and lnBytesLeft members to TY_DEV for
		   line-protocol mode.
02c,15aug84,jlf  changed back to tyLib.h
02b,13aug84,ecs  got rid of S_tyLib_UNKNOWN_REQUEST.
		 changed S_tyLib_READ_ERROR to S_tyLib_SMPTE_READ_ERROR.
02a,10aug84,jlf  changed to tyLib.hx - new mega-file format.
01g,08aug84,ecs  added include of stsLib.h, status codes.
01f,15jun84,dnw  changed TY_DEV to work with new i/o system and ring buffer lib.
01e,27jan84,ecs  added inclusion test.
01d,15sep83,dnw  added xon/xoff stuff: xState field in TY_DEV and definitions
		   of states XST_...
01c,29jul83,dnw	 added ST_TX_CR and removed txStopped.
		 diddled with TY_DEV to make it simpler & more consistent.
		 fiddled with device states to put them in order and
		   confuse the innocent.
01b,22jul83,ecs  added options & status, changed expectCk to TBOOL.
01a,24jun83,ecs  written
*/

#ifndef __INCtyLibh
#define __INCtyLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "ioslib.h"
#include "rnglib.h"
#include "selectlib.h"
#include "vwmodnum.h"
#include "private/semlibp.h"

/* HIDDEN */

typedef struct		/* TY_DEV - tty device descriptor */
    {
    DEV_HDR	devHdr;		/* I/O device header */

    RING_ID	rdBuf;		/* ring buffer for read */
    SEMAPHORE	rdSyncSem;	/* reader synchronization semaphore */
    SEMAPHORE	mutexSem;	/* mutual exclusion semaphore */
    struct			/* current state of the read channel */
	{
	unsigned char xoff;     /* input has been XOFF'd */
	unsigned char pending;  /* XON/XOFF will be sent when xmtr is free*/
        unsigned char canceled; /* read has been canceled */
	unsigned char flushingRdBuf; /* critical section marker */
	}	rdState;

    RING_ID	wrtBuf;		/* ring buffer for write */
    SEMAPHORE	wrtSyncSem;	/* writer synchronization semaphore */
    struct			/* current state of the write channel */
	{
	unsigned char busy;     /* transmitter is busy sending character */
	unsigned char xoff;     /* output has been XOFF'd */
	unsigned char cr;       /* CR should be inserted next (after LF) */
        unsigned char canceled; /* write has been canceled */
	unsigned char flushingWrtBuf; /* critical section marker */
	unsigned char wrtBufBusy; /* task level writing to buffer */
	}	wrtState;

    UINT8	lnNBytes;	/* number of bytes in unfinished new line */
    UINT8	lnBytesLeft;	/* number of bytes left in incompletely
				   dequeued line */
    unsigned short options;	/* options in effect for this channel */
    FUNCPTR	txStartup;	/* pointer to routine to start xmitter */
    FUNCPTR	protoHook;	/* protocol specific hook routine */
    int		protoArg;	/* protocol specific argument */
    SEL_WAKEUP_LIST selWakeupList;/* tasks that are selected on this dev */
    } TY_DEV;

/* END_HIDDEN */

typedef TY_DEV *TY_DEV_ID;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	tyDevInit (TY_DEV_ID pTyDev, int rdBufSize, int wrtBufSize,
			   FUNCPTR txStartup);
extern STATUS 	tyIRd (TY_DEV_ID pTyDev, char inchar);
extern STATUS 	tyITx (TY_DEV_ID pTyDev, char *pChar);
extern STATUS 	tyIoctl (TY_DEV_ID pTyDev, int request, int arg);
extern int 	tyRead (TY_DEV_ID pTyDev, char *buffer, int maxbytes);
extern int 	tyWrite (TY_DEV_ID pTyDev, char *buffer, int nbytes);
extern void 	tyAbortFuncSet (FUNCPTR func);
extern void 	tyAbortSet (char ch);
extern void 	tyBackspaceSet (char ch);
extern void 	tyDeleteLineSet (char ch);
extern void 	tyEOFSet (char ch);
extern void 	tyMonitorTrapSet (char ch);

#else	/* __STDC__ */

extern STATUS 	tyDevInit ();
extern STATUS 	tyIRd ();
extern STATUS 	tyITx ();
extern STATUS 	tyIoctl ();
extern int 	tyRead ();
extern int 	tyWrite ();
extern void 	tyAbortFuncSet ();
extern void 	tyAbortSet ();
extern void 	tyBackspaceSet ();
extern void 	tyDeleteLineSet ();
extern void 	tyEOFSet ();
extern void 	tyMonitorTrapSet ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtyLibh */
