#ifndef __RPC_TYPES_H__
#define __RPC_TYPES_H__
#ident "$Revision: 2.19 $"
/*
 *
 * Copyright 1992, Silicon Graphics, Inc.
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

#ifdef __cplusplus
extern "C" {
#endif

/*	@(#)types.h	1.6 90/07/19 4.1NFSSRC SMI	*/

/* 
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 *      @(#)types.h 1.20 88/02/08 SMI      
 */

/*
 * Rpc additions to <sys/types.h>
 */

/* the rpc headers rely heavily on BSD typedefs that aren't available normally
 * in POSIX mode. We define _BSD_TYPES to getget these useful defines
 */
#define _BSD_TYPES	1
#include <sys/types.h>

/*
 * Also need to include sys/bsd_types.h as the user may have
 * previously included sys/types.h
 */
#include <sys/bsd_types.h>
#include <sys/time.h>

#define	bool_t	int
#define	enum_t	int
#define __dontcare__	-1

#ifndef FALSE
#	define	FALSE	0
#endif

#ifndef TRUE
#	define	TRUE	1
#endif

#ifndef NULL
#	define	NULL	0
#endif

#ifndef _KERNEL
#include <stdlib.h>

#define mem_alloc(bsize)	malloc(bsize)
#define mem_free(ptr, bsize)	free(ptr)
#else
#include "sys/kmem.h"

#define mem_alloc(bsize)	kmem_alloc(bsize, KM_SLEEP)
#define mem_free(ptr, bsize)	kmem_free(ptr, bsize)
#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif /* !__RPC_TYPES_H__ */
