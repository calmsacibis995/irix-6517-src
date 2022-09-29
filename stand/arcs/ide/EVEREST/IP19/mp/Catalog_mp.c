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
*	File:		Catalog_mp.c					*
*									*
\***********************************************************************/

#include <everr_hints.h>
#include <ide_msg.h>
#include "prototypes.h"

#ident	"arcs/ide/EVEREST/IP19/mp/Catalog_mp.c $Revision: 1.2 $"

extern void mp_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t mp_hints [] = {
{MP_EXCP, "mpchk_cache", "Exception occured while verifying scache size\n"},
{MP_CDATA, "mpchk_cache", "pattern mismatch at 0x%x Expect 0x%x Got 0x%x\n"},
{MP_CSIZE, "mpchk_cache", "Actual scache size (%dMB) does not match desired size (%dMB)\n"},
{MP_JUMPER,"mpchk_cache", "Bus tag address jumpers may not be installed for 4MB cache\n"},
{(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0}
};


uint	mp_hintnum[] = { IP19_CACHE, 0 };

catfunc_t mp_catfunc = { mp_hintnum, mp_printf  };
catalog_t mp_catalog = { &mp_catfunc, mp_hints };

void
mp_printf(void *loc)
{
        int *l = (int *)loc;
        msg_printf(ERR, "(IP19 slot:%d cpu:%d)\n", *l, *(l+1));
}
