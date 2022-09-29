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
*	File:		Catalog_io4.c					*
*									*
\***********************************************************************/

#include <everr_hints.h>
#include <ide_msg.h>
#include <prototypes.h>

#ident	"arcs/ide/EVEREST/io4/io4/Catalog_io4.c $Revision: 1.2 $"

extern void io4_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t io4_hints [] = {
{MAPR_GENR, "mapram_test", "Got an exception while running mapram test"},
{MAPR_RDWR, "mapram_test", "Mapram R/W Error:  At 0x%x Wrote 0x%x Read 0x%x\n"},
{MAPR_ADDR, "mapram_test", "Mapram Addr Error: At 0x%x Wrote 0x%x Read 0x%x\n"},
{MAPR_WALK1,"mapram_test", "Mapram Walking 1s: At 0x%x Wrote 0x%x Read 0x%x\n"},
{MAPR_ERR,  "mapram_test", "ERROR bits set with mapram test SUCCESS ??\n"}, 
{(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0}
};


uint	io4_hintnum[] = { IO4_IA, IO4_ID, IO4_MAPRAM, IO4_IBUS, 0 };

catfunc_t io4_catfunc = { io4_hintnum, io4_printf  };
catalog_t io4_catalog = { &io4_catfunc, io4_hints };

void
io4_printf(void *loc)
{
    msg_printf(ERR, "(IO4 slot:%d)\n", *(int *)loc);
}
