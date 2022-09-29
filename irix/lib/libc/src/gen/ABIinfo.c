/* ABIinfo.c
 *
 *      MIPS ABI Function Interface to determine level of support for
 *               particular functions
 *
 * Copyright 1996, Silicon Graphics, Inc.
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
#ident "$Revision: 1.3 $"

/*
 * ABIinfo(3C) - MIPS ABI query function to determine level of support for
 *		 particular functions
 */

#pragma weak ABIinfo	 = _ABIinfo

#include "synonyms.h"
#include <sys/types.h>
#include <ABIinfo.h>
#include <errno.h>

/*
 * ABIinfo will return -1 and set errno to EINVAL is the passed selector 
 * does not correspond to a supported feature.
 */
/* ARGSUSED */
int
ABIinfo(int abi, int selector)
{
	switch(selector) {
	case ABIinfo_BlackBook:
	case ABIinfo_mpconf:
	case ABIinfo_abicc:
	case ABIinfo_XPG:
	case ABIinfo_largefile:
	case ABIinfo_longlong:
	case ABIinfo_X11:
	case ABIinfo_mmap:
		return((int)ConformanceGuide_30);	/* version 3.0 */
	case ABIinfo_backtrace:
	default:
		setoserror(EINVAL);
		return((int)-1);
	}
}
