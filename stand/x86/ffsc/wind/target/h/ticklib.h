/* tickLib.h - header file for tickLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCtickLibh
#define __INCtickLibh

#ifdef __cplusplus
extern "C" {
#endif

/* typedefs */

typedef struct		/* TICK */
    {
    ULONG lower;		/* 00: least significant 32 bits */
    ULONG upper;		/* 04: most significant 32 bits */
    } TICK;

/* variable declarations */

extern ULONG	vxTicks;		/* relative time counter */
extern TICK	vxAbsTicks;		/* abs time since startup in ticks */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void 	tickAnnounce (void);
extern void 	tickSet (ULONG ticks);
extern ULONG 	tickGet (void);

#else

extern void 	tickAnnounce ();
extern void 	tickSet ();
extern ULONG 	tickGet ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtickLibh */
