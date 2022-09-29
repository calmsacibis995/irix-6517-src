/* mmuLib.h - mmuLib header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01i,24nov93,hdn  added prototype mmuLibInit() for I80X86.
01i,02dec93,pme  added Am29K family support.
01h,01oct92,jcf  rolled out 01g to remove warnings.
01g,27sep92,kdl  deleted prototypes for mmu1eLibInit, mmu30LibInit, mmu40LibInit
01f,22sep92,rrr  added support for c++
01e,22sep92,rdc  added support for sun1e.
01d,17sep92,rdc  added prototypes and errors for sun1e.
01c,30jul92,rdc  added prototypes for mmu30LibInit and mmu40LibInit.
01b,28jul92,rdc  removed superfluous function prototypes.
01a,08jul92,rdc  written.
*/

#ifndef __INCmmuLibh
#define __INCmmuLibh

#ifdef __cplusplus
extern "C" {
#endif

#define	S_mmuLib_INVALID_PAGE_SIZE		(M_mmuLib | 1)
#define	S_mmuLib_NO_DESCRIPTOR			(M_mmuLib | 2)
#define	S_mmuLib_INVALID_DESCRIPTOR		(M_mmuLib | 3)

/* sun4 specific codes */

#define	S_mmuLib_OUT_OF_PMEGS			(M_mmuLib | 5)

/* virtual addresses must be in the first or last 1/2 Gibabyte of
 * virtual address space
 */

#define	S_mmuLib_VIRT_ADDR_OUT_OF_BOUNDS	(M_mmuLib | 6)

typedef struct mmuTransTblStruct *MMU_TRANS_TBL_ID;

#if defined(__STDC__) || defined(__cplusplus)

STATUS mmu30LibInit (int pageSize);
STATUS mmu40LibInit (int pageSize);
STATUS mmu1eLibInit (int pageSize);
STATUS mmuLibInit (int pageSize);
STATUS mmuAm29kLibInit (int pageSize);

#else	/* __STDC__ */

STATUS mmu30LibInit ();
STATUS mmu40LibInit ();
STATUS mmu1eLibInit ();
STATUS mmuLibInit ();
STATUS mmuAm29kLibInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmmuLibh */
