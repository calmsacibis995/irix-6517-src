
/*
 *
 * Cosmo2 CGI1 - Initialization 
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

#include "cosmo2_defs.h"

#if !defined(IP30)
#include <sys/mc.h>

void
cos2_Set64Bit(void) 
{

	u_int gio64_arb_reg, gio64_arb_reg_sav;
	u_int mask,flags;

        mask = (GIO64_ARB_EXP0_SIZE_64 | GIO64_ARB_EXP0_RT |
                GIO64_ARB_EXP0_MST);
        flags=  GIO64_ARB_EXP0_SIZE_64 | GIO64_ARB_EXP0_RT |
                        GIO64_ARB_EXP0_MST | GIO64_ARB_EXP0_PIPED;

  	gio64_arb_reg_sav = *(volatile u_int *)PHYS_TO_K1(GIO64_ARB);

        /* EISA options have to be set, otherwise it won't work
           UNIX did the same thing, reason unknown */
        gio64_arb_reg = (gio64_arb_reg_sav    & ~mask) | (flags & mask);

        writemcreg(GIO64_ARB, gio64_arb_reg);

#if 0
	cosmobase = gio_slot[0].base_addr;
	cgi1ptr = (cgi1_reg_t *) cosmobase ;
#endif


}
#endif
