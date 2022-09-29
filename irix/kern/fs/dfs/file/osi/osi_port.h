/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1994, 1989 Transarc Corporation - All rights reserved */

#ifndef TRANSARC_OSI_PORT_H
#define	TRANSARC_OSI_PORT_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/param.h>

#ifndef KERNEL
#define copyin(s, d, l)		(bcopy(s, d, l), /*return value*/0)
#define copyout(s, d, l)	copyin(s, d, l)
#define osi_copyin(s, d, l)	copyin(s, d, l)
#define osi_copyout(s, d, l)	copyout(s, d, l)

#define osi_assert(x) \
    (!(x) ? (fprintf (stderr, "assertion failed: line %d, file %s\n",\
                      __LINE__,__FILE__), fflush(stderr), abort(), 0) : 0)

#define osi_GetMachineName(buf, size) gethostname(buf, size)
#endif
#if defined(SGIMIPS) && defined(KERNEL)
#define osi_assert(x) \
    (!(x) ? (printf ("assertion failed: line %d, file %s\n",\
                    __LINE__,__FILE__), panic("assert"), 0) : 0)
#endif


/*
 * Macro for setlocale call
 */
#define osi_setlocale(category, locale) setlocale(category, locale)

#include <dcedfs/osi_port_mach.h>

#endif	/* !TRANSARC_OSI_PORT_H */
