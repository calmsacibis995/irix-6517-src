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
*	File:		Catalog_enet.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <err_hints.h>
#include <everr_hints.h>
#include <ide_msg.h>


extern void enet_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t enet_hints [] = {
{ENET_XMIT, "enet", 
	"Having trouble transmitting packets. Check EPC control signals\n"},
{ENET_RCV, "enet", 
"Not receiving packets. Check SEEQ_INTR and other SEEQ/EPC control signals\n"},
{ENET_DATA, "enet", 
	"Bad data in byte %d (%d bytes total), Expected 0x%x, Got 0x%x\n"},
{ENET_TSTAT, "enet", 
	"TSTAT register bits %d:%d, Expected 0x%x, Got 0x%x\n"},
{ENET_RSTAT, "enet", 
	"RSTAT register bits %d:%d, Expected 0x%x, Got 0x%x\n"},
(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0
};



uint	enet_hintnum[] = { IO4_EPC, 0 };
catfunc_t enet_catfunc = { enet_hintnum, enet_printf  };
catalog_t enet_catalog = { &enet_catfunc, enet_hints };

void
enet_printf(void *loc)
{
	msg_printf(ERR, "(IO4 slot: %x)\n", *(int *)loc);
}
