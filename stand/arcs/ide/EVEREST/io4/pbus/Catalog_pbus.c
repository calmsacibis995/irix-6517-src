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
*	File:		Catalog_pbus.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <err_hints.h>

extern void pbus_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t pbus_hints [] = {

(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0
};



uint	pbus_hintnum[] = { IO4_EPC, 0 };
catfunc_t pbus_catfunc = { pbus_hintnum, pbus_printf  };
catalog_t pbus_catalog = { &pbus_catfunc, pbus_hints };

void
pbus_printf(void *loc)
{

}
