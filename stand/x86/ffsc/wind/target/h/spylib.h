/* spyLib.h - header file for spyLib.c */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,27may95,p_m  added spyLibInit() and allow decoupling of the result printing.
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCspyLibh
#define __INCspyLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void 	spyLibInit (void);
extern STATUS 	spyClkStartCommon (int intsPerSec, FUNCPTR printRtn);
extern void 	spyCommon (int freq, int ticksPerSec, FUNCPTR printRtn);
extern void 	spyClkStopCommon (void);
extern void 	spyReportCommon (FUNCPTR printRtn);
extern void 	spyStopCommon (void);
extern void 	spyComTask (int freq, FUNCPTR printRtn);

#else	/* __STDC__ */

extern void 	spyLibInit ();
extern STATUS 	spyClkStartCommon ();
extern void 	spyCommon ();
extern void 	spyClkStopCommon ();
extern void 	spyReportCommon ();
extern void 	spyStopCommon ();
extern void 	spyComTask ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCspyLibh */
