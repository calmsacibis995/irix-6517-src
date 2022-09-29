/* kernelLib.h - header file for kernelLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCkernelLibh
#define __INCkernelLibh

#ifdef __cplusplus
extern "C" {
#endif

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern char *	kernelVersion (void);
extern STATUS 	kernelTimeSlice (int ticks);
extern void 	kernelInit (FUNCPTR rootRtn, unsigned rootMemSize,
			    char *pMemPoolStart, char *pMemPoolEnd,
			    unsigned intStackSize, int lockOutLevel);
#else

extern char *	kernelVersion ();
extern STATUS 	kernelTimeSlice ();
extern void 	kernelInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCkernelLibh */
