/* workQLibP.h - private kernel work queue header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,04jul92,jcf   extracted from v2i of wind.h.
*/

#ifndef __INCworkQLibPh
#define __INCworkQLibPh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE
#include "vxworks.h"
#include "vwmodnum.h"

/* typedefs */

typedef struct		/* JOB */
    {
    FUNCPTR function;		/* 00: function to invoke */
    int	arg1;			/* 04: argument 1 */
    int arg2;			/* 08: argument 2 */
    int arg3;			/* 12: argument 3 (unused) */
    } JOB;

#define WIND_JOBS_MAX	64		/* max q'ed jobs must be 64 */

/* variable declarations */

extern UINT8	workQReadIx;		/* circular work queue read index */
extern UINT8	workQWriteIx;		/* circular work queue read index */
extern BOOL	workQIsEmpty;		/* TRUE if work queue is empty */
extern int	pJobPool[];		/* pool of memory for jobs */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void	workQInit (void);
extern void	workQDoWork (void);
extern void	workQPanic (void);
extern void	workQAdd0 (FUNCPTR func);
extern void	workQAdd1 (FUNCPTR func, int arg1);
extern void	workQAdd2 (FUNCPTR func, int arg1, int arg2);

#else

extern void	workQInit ();
extern void	workQDoWork ();
extern void	workQPanic ();
extern void	workQAdd0 ();
extern void	workQAdd1 ();
extern void	workQAdd2 ();

#endif	/* __STDC__ */

#else	/* _ASMLANGUAGE */

#define JOB_FUNCPTR	0x0		/* routine to invoke */
#define JOB_ARG1	0x4		/* argument 1 */
#define JOB_ARG2	0x8		/* argument 2 */
#define JOB_ARG3	0xc		/* argument 3 */

#define WORK_READ_IX	0x0		/* work queue read index */
#define WORK_WRITE_IX	0x4		/* work queue write index */
#define WORK_JOB_BASE	0x10		/* pointer to job pool */

#define WORK_MAX_IX	0x3f		/* maximum value work index can be */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCworkQLibPh */
