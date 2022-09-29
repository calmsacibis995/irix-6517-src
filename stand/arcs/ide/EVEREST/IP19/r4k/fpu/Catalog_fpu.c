/*
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

/***********************************************************************\
*	File:		Catalog_fpu.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <everr_hints.h>
#include <ide_msg.h>

#ident	"arcs/ide/EVEREST/IP19/r4k/fpu/Catalog_fpu.c $Revision: 1.3 $"

extern void fpu_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t fpu_hints [] = {
{FADDSUBD_RES, "faddsubd", "FP double add/sub result error : Expected 0x%x Got 0x%x\n"},
{FADDSUBD_STS, "faddsubd", "FP double add/sub status error : Expected 0 Got 0x%x\n"},
{FADDSUBD_FIX, "faddsubd", "Fixed to double conversion failed : Before 0x%x After 0x%x\n"},
{FADDSUBS_RES, "faddsubs", "FP single add/sub result error : Expected 0x%x Got 0x%x\n"},
{FADDSUBS_STS, "faddsubs", "FP single add/sub status error : Expected 0 Got 0x%x\n"},
{FADDSUBS_FIX, "faddsubs", "Fixed to single conversion failed : Before 0x%x After 0x%x\n"},
{FDIVZRO_STS, "fdivzero", "Divide by Zero exception status error : 0x%x\n"},
{FDIVZRO_DIVD, "fdivzero", "Dividend conversion failed : Before 0x%x After 0x%x\n"},
{FDIVZRO_DIVS, "fdivzero", "Divisor conversion failed : Before 0x%x After 0x%x\n"},
{FINEX_STS, "finexact", "Inexact exception status error : 0x%x\n"},
{FINV_NOEXCP, "finvalid", "Invalid exception didn't occur\n"},
{FINV_STS, "finvalid", "Invalid exception status error : 0x%x\n"},
{FINV_FIX, "finvalid", "Invalid exception dividend error : Expected 0x%x Got 0x%x\n"},
{FCMPUT_UNEXP, "fpcmput", "FP computation unexpected exception : 0x%x\n"},
{FMULDIVD_DIV, "fmuldivd", "FP double divide result error : Expected 0x%x Got 0x%x\n"},
{FMULDIVD_MUL, "fmuldivd", "FP double multiply result error : Expected 0x%x Got 0x%x\n"},
{FMULDIVS_DIV, "fmuldivs", "FP single divide result error : Expected 0x%x Got 0x%x\n"},
{FMULDIVS_MUL, "fmuldivs", "FP single multiply result error : Expected 0x%x Got 0x%x\n"},
{FMULSUBD_RES, "fmulsubd", "FP double mul/sub result error : Expected 0x%x Got 0x%x\n"},
{FMULSUBD_FIX, "fmulsubd", "Fixed to double conversion failed : Before 0x%x After 0x%x\n"},
{FMULSUBD_STS, "fmulsubd", "FP double mul/div status error : 0x%x\n"},
{FMULSUBS_RES, "fmulsubs", "FP single mul/div result error : Expected 0x%x Got 0x%x\n"},
{FMULSUBS_FIX, "fmulsubs", "Fixed to single conversion failed : Before 0x%x After 0x%x\n"},
{FMULSUBS_STS, "fmulsubs", "FP single mul/div status error : 0x%x\n"},
{FOVER_STS, "foverflow", "Overflow exception status error : 0x%x\n"},
{FCOMPUTE_S, "fpcmput", "Single precision %s error : Expected 0x%x Got 0x%x\n"},
{FCOMPUTE_D, "fpcmput", "Double precision %s error : Expected 0x%x 0x%x Got 0x%x 0x%x\n"},
{FPMEM_DATA, "fpmem", "Load/store FP reg %d data error : Expected 0x%x Got 0x%x\n"},
{FPMEM_INVD, "fpmem", "Load/store FP reg %d inverted data error : Expected 0x%x, Got 0x%x\n"},
{FPREG_DATA, "fpregs", "FP register %d data error : Expected 0x%x Got 0x%x\n"},
{FPREG_INVD, "fpregs", "FP register %d inverted data error : Expected 0x%x Got 0x%x\n"},
{FUNDER_EXCP, "funderflow", "Exception other than Underflow in FCR31 : 0x%x\n"},
{FUNDER_NOEXCP, "funderflow", "Failed to generate Underflow Exception\n"},
{(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0}
};


uint	fpu_hintnum[] = { IP19_FPU, 0 };

catfunc_t fpu_catfunc = { fpu_hintnum, 0  };
catalog_t fpu_catalog = { &fpu_catfunc, fpu_hints };

void
fpu_printf(void *loc)
{
	int *l = (int *)loc;
	msg_printf(ERR, "(IP19 slot:%d cpu:%d)\n", *l, *(l+1));
}
