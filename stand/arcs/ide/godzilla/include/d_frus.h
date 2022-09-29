#ifndef __IDE_FRUS_H__
#define __IDE_FRUS_H__
/*
 * d_frus.h
 *
 * 	IDE fru's header (FRU = Field Replacable Unit)
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ident "ide/godzilla/include/d_frus.h:  $Revision: 1.4 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/*
 * FRU definitions: NOTE these are not in d_godzilla, so that 
 *			they stay with the fru names
 */
#define	D_FRU_PMXX		0x0
#define	D_FRU_IP30		0x1
#define	D_FRU_FRONT_PLANE	0x2
#define	D_FRU_PCI_CARD		0x3
#define D_FRU_MEM		0x4
#define D_FRU_GFX		0x5
#define GODZILLA_FRUS  		0x6	/* last D_FRU + 1 */

#if DEFINE_FRU_DATA	/* set in stand/arcs/ide/godzilla/uif/status.c */

/* fru names: should match the code above */
char    *godzilla_fru_names [] = {
	"PROCESSOR MODULE",
	"IP30",
	"FRONT_PLANE",
	"PCI_CARD",
	"MEMORY DIMMs",
	"GRAPHICS BOARD",
};
#else
extern  char    *godzilla_fru_names [];
#endif

/* to print NIC info with FRU */
extern	void	print_nic_info(int fru_number, int slot);

#endif /* C || C++ */

#endif  /* __IDE_GODZILLA_H__ */

