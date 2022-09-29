/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _KSYS_EXCEPTION_H_
#define _KSYS_EXCEPTION_H_

#ident	"$Revision: 1.8 $"

#include <sys/pcb.h>
#include <sys/immu.h>	/* for pde_t */
#include <sys/reg.h>
#include <sys/sbd.h>	/* for NWIREDENTRIES */

#if R4000 || R10000
#define u_ubptbl        u_pde.u_tlbpde_ubptbl   /* A pair of pde_t entries */
#endif
#if TFP || BEAST
#define u_ubptbl        u_pde.u_pde_ubptbl
#endif

typedef	struct exception {
	pcb_t		u_pcb;		/* MUST be first */
	eframe_t	u_eframe;	/* saved exception state */

	/* The following 3 fields only referenced if !NO_WIRED_SEGMENTS:
	 *	u_pde, u_tlbhi_tbl, u_nexttlb
	 * NOTE: We define the following fields if NWIREDENTRIES is defined
	 * rather than testing for !NO_WIRED_SEGMENTS.  This is so we can
	 * define these fields as placeholders in the uarea even if we are
	 * executing without wired segments.
	 */
#if NWIREDENTRIES
	union {
		pde_t	u_pde_ubptbl[NWIREDENTRIES-TLBWIREDBASE];
					/* page table entries for the	*/
					/* page tables, et al,	*/
					/* to avoid double misses */
		tlbpde_t u_tlbpde_ubptbl[NWIREDENTRIES-TLBWIREDBASE];
	} u_pde;
	caddr_t		u_tlbhi_tbl[NWIREDENTRIES-TLBWIREDBASE];
					/* virtual addresses to go with */
					/* above page table		*/
	u_char		u_nexttlb;	/* index for next available	*/
					/* entry in above tables	*/
#endif	/* NWIREDENTRIES */
} exception_t;

#define	curexceptionp	curuthread->ut_exception

#endif /* _KSYS_EXCEPTION_H_ */
