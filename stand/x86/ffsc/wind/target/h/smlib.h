/* smLib.h - include file for VxWorks shared memory common library */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01q,29jan93,pme  changed SM_ALIGN_BOUNDARY to 16.
01p,22sep92,rrr  added support for c++
01o,12sep92,ajm  moved previously redundant define DEFAULT_BEATS_TO_WAIT from
		 smObjLib.h, and smPktLib.h
01n,28jul92,pme  added SM_ALIGN_BOUNDARY.
01m,24jul92,elh  Moved heartbeat from anchor.
01l,19jul92,pme  added external declaration of smCurMaxTries.
01k,23jun92,elh  added SM_ constants from if_bp, cleanup.
01j,22jun92,rrr  fix decl of smLibInit when __STDC__ is not defined
01i,02jun92,elh  the tree shuffle
01h,28may92.elh  General clean up.
01g,20may92,pme+ Split even further.  Removed USE_OFFSET.
	    elh	 Moved pkt specific stuff to smPktLib.h.
01f,13may92,pme  Added smObjHeaderAdrs fields to anchor.
01e,03may92,elh  Added smNetReserved fields to anchor.
01d,01apr92,elh	 Removed references to shMemHwTasFunc and
		 shMemIntGenFunc (now in smUtilLib).
		 Removed interrupt types.
01c,04feb92,elh	 ansified
01b,17dec91,elh  externed hooks, added ifdef around file, changed VOID
		 to void.  Added masterCpu, user1 and user2, to SM_ANCHOR.
		 Added S_shMemLib_MEMORY_ERROR. Changed copyright.
01a,15aug90,kdl	 written.
*/

#ifndef __INCsmLibh
#define __INCsmLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "vwmodnum.h"


/* defines */

/* Error codes */

#define S_smLib_MEMORY_ERROR			(M_smLib | 1)
#define S_smLib_INVALID_CPU_NUMBER		(M_smLib | 2)
#define S_smLib_NOT_ATTACHED			(M_smLib | 3)
#define S_smLib_NO_REGIONS			(M_smLib | 4)

/* Miscellaneous Constants */

#define	SM_READY		((UINT) 0x87654321)
					/* value set in anchor when init'd */
#define SM_MASTER		0	/* default CPU 0 is master */

#define SM_ALIGN_BOUNDARY   	16 	/* shared memory address alignement */

/* default values */

#define SM_REGIONS_MAX		20	/* max number of regions */
#define DEFAULT_ALIVE_TIMEOUT	600	/* default seconds to wait for h'beat */
#define DEFAULT_BEATS_TO_WAIT   5       /* beats to wait 'til believe sm dead */

/* Test-and-set type values */

#define	SM_TAS_SOFT		0	/* software test-and-set */
#define	SM_TAS_HARD		1	/* hardware test-and-set */

/* CPU status values */

#define	SM_CPU_ATTACHED       	1	/* cpu attached to shared mem */
#define	SM_CPU_NOT_ATTACHED   	0	/* cpu not attached */

/* backplane interrupt methods */

#define SM_INT_NONE		0	/* no interrupt - poll instead */

					/* mailbox write interrupts */
#define SM_INT_MAILBOX_1	1	/* write byte 		    */
#define SM_INT_MAILBOX_2	2	/* write word 		    */
#define SM_INT_MAILBOX_4	3	/* write long 		    */

					/* arg1 = bus address space
					 * arg2 = bus address
					 * arg3 = value
					 */

#define SM_INT_BUS		4	/* vme bus interrupt 	     */
					/*    arg1 = interrupt level
					 *    arg2 = interrupt vector
					 *    arg3 = N/A
					 */

					/* mailbox read interrupts    */
					/*    arg1 = bus address space
					 *    arg2 = bus address
					 *    arg3 = value
					 */
#define SM_INT_MAILBOX_R1	5	/* read byte 		*/
#define SM_INT_MAILBOX_R2	6	/* read word 		*/
#define SM_INT_MAILBOX_R4	7	/* read long 		*/

#define SM_OFFSET_TO_LOCAL(offset , baseAddr, typeCast) \
		((typeCast) ((char *) offset +  baseAddr))

#define SM_LOCAL_TO_OFFSET(localAdrs, baseAddr) \
		((int) ((char *) localAdrs - baseAddr))


#ifndef _ASMLANGUAGE

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1				/* don't to optimize alignments */
#endif /* CPU_FAMILY==I960 */

/* CPU Descriptor */

typedef struct sm_cpu_desc	/* SM_CPU_DESC */
    {
    int		status;		/* CPU status - attached/unattached */
    int		intType;	/* interrupt type */
    int		intArg1;	/* interrupt argument #1 */
    int		intArg2;	/* interrupt argument #2 */
    int		intArg3;	/* interrupt argument #3 */
    int		reserved1;	/* (future use) */
    int		reserved2;	/* (future use) */
    } SM_CPU_DESC;


/* Shared Memory Header */

typedef struct sm_hdr		/* SM_HDR */
    {
    int		tasType;	/* test-and-set type (test-and-set-and-check?)*/
    int		maxCpus;	/* maximum number of CPU's in table */
    int		cpuTable;	/* CPU descriptor table (offset) */
    int		reserved1;	/* (future use) */
    int		reserved2;	/* (future use) */
    int		reserved3;	/* (future use) */
    int		reserved4;	/* (future use) */
    int		reserved5;	/* (future use) */
    int		reserved6;	/* (future use) */
    int		reserved7;	/* (future use) */
    int		reserved8;	/* (future use) */
    } SM_HDR;


/* Shared Memory Anchor */

typedef struct sm_anchor	/* SM_ANCHOR */
    {
    UINT	readyValue;     /* set to SM_READY when sh mem init'd */
    int		version;        /* version number */
    int		masterCpu;      /* shared memory master */
    int		smHeader;	/* offset of shared mem header */
    int		smPktHeader;    /* offset of smPkt header */
    int 	smObjHeader;    /* offset of smObj header */
    int		user1;          /* user defined space 1 */
    int		user2;          /* user defined space 2 */
    int		reserved1;      /* (future use) */
    int		reserved2;      /* (future use) */
    int		reserved3;      /* (future use) */
    } SM_ANCHOR;

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0				/* turn off alignment requirement */
#endif /* CPU_FAMILY==I960 */


/* Shared Memory Descriptor (maintained by each CPU) */

typedef struct sm_desc		/* SM_DESC */
    {
    int		status;		/* cpu status (attached/not attached) */
    SM_ANCHOR	*anchorLocalAdrs;/* local addr of sh mem anchor */
    SM_HDR	*headerLocalAdrs;/* local addr of sh mem header */
    SM_CPU_DESC	*cpuTblLocalAdrs;/* local addr of cpu descriptor tbl */
    int		base;		/* base address */
    int		cpuNum;		/* this CPU's number */
    int		ticksPerBeat;	/* cpu ticks per sh mem heartbeat */
    int		intType;	/* interrupt type */
    int		intArg1;	/* interrupt argument #1 */
    int		intArg2;	/* interrupt argument #2 */
    int		intArg3;	/* interrupt argument #3 */
    int		maxCpus;	/* max number of CPU's using sh mem */
    FUNCPTR	tasRoutine;	/* addr of TAS routine to use */
    FUNCPTR	tasClearRoutine;/* addr of TAS clear routine to use */
    } SM_DESC;


/* Shared Memory Information Structure */

typedef struct sm_info		/* SM_INFO */
    {
    int		version;	/* shared memory protocol version */
    int		tasType;	/* test-and-set method */
    int		maxCpus;	/* maximum number of cpu's */
    int		attachedCpus;	/* number of cpu's currently attached */
    } SM_INFO;


/* CPU Information Structure */

typedef struct sm_cpu_info	/* SM_CPU_INFO */
    {
    int		cpuNum;		/* cpu number */
    int		status;		/* cpu status - attached/unattached */
    int		intType;	/* interrupt type */
    int		intArg1;	/* interrupt argument #1 */
    int		intArg2;	/* interrupt argument #2 */
    int		intArg3;	/* interrupt argument #3 */
    } SM_CPU_INFO;

typedef struct sm_region	/* shared memory region */
    {
    SM_ANCHOR *	anchor;		/* shared memory anchor */
    } SM_REGION;

/* Variable Declaration */

extern int	smCurMaxTries;    /* current maximum # of tries to get lock */

/* Function Declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	smSetup (SM_ANCHOR *anchorLocalAdrs, char *smLocalAdrs,
			 int tasType, int maxCpus, int	* pMemUsed);
extern STATUS	smAttach (SM_DESC *pSmDesc);
extern STATUS 	smDetach (SM_DESC *pSmDesc);
extern void 	smInit (SM_DESC *pSmDesc, SM_ANCHOR *anchorLocalAdrs,
			int ticksPerBeat, int intType, int intArg1,
			int intArg2, int intArg3);
extern BOOL 	smIsAlive (SM_ANCHOR *pAnchor, int *pHeader, int base,
			   int heartBeats, int ticksPerBeat);
extern STATUS 	smLockTake (int * lockLocalAdrs, FUNCPTR tasRoutine,
			   int numTries, int * pOldLvl);
extern void	smLockGive (int *lockLocalAdrs, FUNCPTR tasClearRoutine,
			    int oldLvl);
extern STATUS 	smInfoGet (SM_DESC *pSmDesc, SM_INFO *pInfo);
extern STATUS 	smCpuInfoGet (SM_DESC *pSmDesc, int cpuNum,
			      SM_CPU_INFO *pCpuInfo);


#else	/* __STDC__ */

extern STATUS 	smSetup ();
extern STATUS	smAttach ();
extern STATUS 	smDetach ();
extern void	smInit ();
extern BOOL 	smIsAlive ();
extern STATUS 	smLockTake ();
extern void	smLockGive ();
extern STATUS 	smInfoGet ();
extern STATUS 	smCpuInfoGet ();

#endif	/* __STDC__ */

#endif  /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmLibh */
