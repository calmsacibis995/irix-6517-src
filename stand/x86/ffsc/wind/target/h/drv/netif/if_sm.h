/* if_sm.h - WRS backplane driver header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,16jun92,elh  clean up.  Added numLoan to smIfAttach.
01d,02jun92,elh  the tree shuffle
01c,27may92,elh  Incorperated the changes caused from restructuring the
		 shared memory libraries.
01b,10apr92,elh  added masterAddr to store master address.
01a,17nov90,elh  written.
*/

#ifndef __INCif_smh
#define __INCif_smh

#ifdef __cplusplus
extern "C" {
#endif

#include "smpktlib.h"

/* defines */

#define xs_if			xs_ac.ac_if	/* interface structure */
#define xs_enaddr		xs_ac.ac_enaddr	/* ethernet address */

#define smIfInputCount(xs, cpu) 		\
	(xs)->smPktDesc.cpuLocalAdrs [(cpu)].inputList.count

/* typedefs */

typedef struct sm_softc
    {
    struct arpcom	xs_ac;			/* common ethernet structure*/
    SM_PKT_DESC		smPktDesc;		/* shared mem packet desc */
    int			taskRecvActive;		/* recv task active */
    u_long		masterAddr;		/* master address */
    int			bufFree;		/* buffer loaning info */
    } SM_SOFTC;

/* function prototypes */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS smIfAttach (int unit, SM_ANCHOR * pAnchor, int maxInputPkts,
			  int intType, int intArg1, int intArg2, int intArg3,
			  int ticksPerBeat, int numLoan);
extern void smIfInput (SM_SOFTC * xs);
extern void smIfLoanReturn (SM_SOFTC * xs, SM_PKT * pPkt);

#else

extern STATUS 		smIfAttach ();
extern void		smIfInput ();
extern void		smIfLoanReturn ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_smh */
