/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.4 $"

#ifndef	_HUBDIAGS_H_
#define	_HUBDIAGS_H_

#define BTE0_DONE_INT		63		       /* bte0 done int */
#define BTE1_DONE_INT		62		       /* bte1 done int */
#define BTE0_SOURCE		DIAG_BASE	       /* bte0 pulladdr */
#define BTE1_SOURCE 		(DIAG_BASE + 0x080000) /* bte1 pulladdr */
#define BTE0_DEST		(DIAG_BASE + 0x100000) /* bte0 pushaddr */
#define BTE1_DEST       	(DIAG_BASE + 0x180000) /* bte1 pushaddr */
#define BTE_CPY_LENGTH		0x080000 	       /* xfer half a meg */
#define SOURCE0_VALUE		0xC3C3
#define SOURCE1_VALUE		0xA5A5
#define	DEST_VALUE		0xDEAD
#define IBLS_VALUE		0x0000000000101000	/* 4096 cache lines */
#define IBCT_VALUE		0x0000000000000000	/* interrupt mode */
#define IBIA0_VALUE		0x00000000003f0000	/* int level, node id */
#define IBIA1_VALUE		0x00000000003e0000	/* int level, node id */
#define MAX_XFER_TIME		(30 * 1000000)  	/* usec */

/* BTE is busy iff there was no error and the the length is non-zero */
#define BTE_IS_BUSY(_x) (!((_x) & IBLS_ERROR) && ((_x) & IBLS_LENGTH_MASK))

#define BTE_LOAD(_x)            (*(volatile hubreg_t *)(bte_base + (_x)))
#define BTE_STORE(_x, _y)       BTE_LOAD(_x) = (_y)

int hub_intrpt_diag(int diag_mode);
int hub_bte_diag(int diag_mode);

#endif /* _HUBDIAGS_H_ */
