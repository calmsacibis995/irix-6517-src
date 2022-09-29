/* types.h - basic unix types header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * and the VxWorks Software License Agreement specify the terms and
 * conditions for redistribution.
 *
 *	@(#)types.h	7.1 (Berkeley) 6/4/86
 */

/*
modification history
--------------------
02j,22sep92,rrr  added support for c++
02i,03jul92,smb  moved typedefs to types/vxTypes.h.
02h,26may92,rrr  the tree shuffle
02g,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
02f,25oct90,dnw  added test for SunOS 4.1 sys/types.h.
02e,05oct90,dnw  added ifdef around size_t.
02d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02c,21feb90,dab  added typedef for addr_t.
02b,29mar88,gae  added necessary junk BSD4.3 style select().
02a,29apr87,dnw  removed unnecessary junk.
		 added header and copyright.
*/

#ifndef __INCtypesh
#define __INCtypesh

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxtypes.h"

#ifdef __cplusplus
}
#endif

#endif /* __INCtypesh */
