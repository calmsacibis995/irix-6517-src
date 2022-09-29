/* if_bp.c - backplane network header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01i,22sep92,rrr  added support for c++
01h,15sep92,jcf  added prototypes.
01g,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01c,08nov89,shl  added ifdef to prevent inclusion of vxWorks.h more than once.
01b,06mar89,jcf  added read-location mailbox types.
01a,14jun87,dnw  written
*/

#ifndef __INCif_bph
#define __INCif_bph

#ifdef __cplusplus
extern "C" {
#endif

/* backplane interrupt methods */

#define BP_INT_NONE		0	/* no interrupt - poll instead */

#define BP_INT_MAILBOX_1	1	/* mailbox write byte to bus adrs */
#define BP_INT_MAILBOX_2	2	/* mailbox write word to bus adrs */
#define BP_INT_MAILBOX_4	3	/* mailbox write long to bus adrs */
					/*    arg1 = bus address space
					 *    arg2 = bus address
					 *    arg3 = value
					 */

#define BP_INT_BUS		4	/* generate bus interrupt */
					/*    arg1 = interrupt level
					 *    arg2 = interrupt vector
					 */

#define BP_INT_MAILBOX_R1	5	/* mailbox read byte to bus adrs */
#define BP_INT_MAILBOX_R2	6	/* mailbox read word to bus adrs */
#define BP_INT_MAILBOX_R4	7	/* mailbox read long to bus adrs */
					/*    arg1 = bus address space
					 *    arg2 = bus address
					 *    arg3 = value
					 */
/* function prototypes */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	bpattach (int unit, char *pAnchor, int procNum, int intType,
			  int intArg1, int intArg2, int intArg3);
extern STATUS	bpInit (char *pAnchor, char *pMem, int memSize, BOOL tasOK);
extern void	bpShow (char *bpName, BOOL zero);

#else	/* __STDC__ */

extern STATUS	bpattach ();
extern STATUS	bpInit ();
extern void	bpShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_bph */
