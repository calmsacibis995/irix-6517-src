/* _sqrt_s & _sqrt_d - floating square root 
 *	libc/src/math/sqrt.s
 *	
 * Copyright 1995, Silicon Graphics, Inc.
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

/*
 * 
 * This is to define routines needed for the ABI ...
 * The defines are in src/abi/abi_catchall.c, but the real routines
 * are here.  The Makefile in this directory will not build
 * sqrt.s so we don't get duplicate symbols in /usr/lib/abi/libc.so.
 *
 * The error routines have been left out since the ABI is sketchy 
 * on the errno that should be returned ... This even with a negative
 * values these routines will return a value (ie. nan value returned)
 * 
 */

#include <regdef.h>
#include <sys/asm.h>
#include <errno.h>


LEAF(_sqrt_s)
	SETUP_GP
	USE_ALT_CP(t2)
	SETUP_GP64(t2,_sqrt_s)
	sqrt.s $f0,$f12
	j	ra
	END(_sqrt_s)

LEAF(_sqrt_d)
	SETUP_GP
	USE_ALT_CP(t2)
	SETUP_GP64(t2,_sqrt_d)
	sqrt.d $f0,$f12
	j	ra
	END(_sqrt_d)
