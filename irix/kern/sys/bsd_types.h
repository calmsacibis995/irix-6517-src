/*
 * Copyright 1990,1992 Silicon Graphics, Inc.
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

#ifndef _SYS_BSD_TYPES_H
#define _SYS_BSD_TYPES_H

#ident	"$Revision: 1.2 $"

/*
 * These typedefs are needed by lots of BSD derived code - especially
 * networking. Alas these are not POSIX compliant so cannot be part of
 * sys/types.h
 */
typedef	struct { int r[1]; } *	physadr;
typedef	unsigned char	unchar;
typedef	unsigned char	u_char;
typedef	unsigned short	ushort;
typedef	unsigned short	u_short;
typedef	unsigned int	uint;
typedef	unsigned int	u_int;
typedef	unsigned long	ulong;
typedef	unsigned long	u_long;
typedef	struct	_quad { long val[2]; } quad;

#if defined(_BSD_COMPAT)
/* get major/minor functions */
#include <sys/mkdev.h>
#endif

#include <sys/select.h>

#endif
