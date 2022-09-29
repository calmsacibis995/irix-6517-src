/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.1 $"


#define UPDATE_ON_WRITE 0x30
#define	VPN2	0xc0304000	/* virtual page we are using 	*/
#define PHYPAGE 0x00200000	/* physical address in memory	*/
#define ASID0	0x0		/* */
#define NUMWRDS	0x60		/* Number of words to test	*/
#define TLBLOC	0x10		/* Tlb we are using		*/


	 /* These should be moved to an .h file eventually */
#define  PState_Invalid           0x00000000         
#define  PState_Clean_Exclusive   0x00000080
#define  PState_Dirty_Exclusive   0x000000C0
#define  PState_Shared            0x00000040
#define  PState_Mask		  0x000000c0
#define  CONFIG_DB_SHIFT	4
#define  CONFIG_IB_SHIFT	5

	 /* Masks and defines for the Secondary */

#define  SCState_Invalid	   0x00000000
#define  SCState_Clean_Exclusive   0x00001000
#define  SCState_Dirty_Exclusive   0x00001400
#define  SCState_Shared            0x00001800
#define  SCState_Dirty_Shared      0x00001C00
#define  SCState_Mask		   0x00001c00

#define CACHESIZ 	0xa0000300
#define PCSTATE_ERR	0x1
#define PTAG_ERR	0x2
#define SCSTATE_ERR	0x3
#define SCTAG_ERR	0x4
#define VIRTINDX_ERR	0x5

#define EXPADR		0x0
#define EXPADR_INDX	0x0
#define EXPCS		0x4
#define EXPCS_INDX	0x1
#define ACTTAG		0x8
#define ACTTAG_INDX	0x2
#define VIRTADR		0xc
#define VIRTADR_INDX	0x3
