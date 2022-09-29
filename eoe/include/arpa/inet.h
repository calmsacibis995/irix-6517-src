#ifndef __ARPA_INET_H__
#define __ARPA_INET_H__
/*
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

#ident "$Revision: 1.12 $"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)inet.h	5.4 (Berkeley) 6/1/90
 */

/* External definitions for functions in inet(3N) */

#include <standards.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/endian.h>

extern in_addr_t	inet_addr(const char *);
extern in_addr_t        inet_lnaof(struct in_addr);
extern struct in_addr   inet_makeaddr(in_addr_t, in_addr_t);
extern in_addr_t        inet_netof(struct in_addr);
extern in_addr_t        inet_network(const char *);
extern char *           inet_ntoa(struct in_addr);

#if _SGIAPI
extern int		inet_aton(const char *, struct in_addr *);
extern int		inet_pton(int, const char *, void *);
extern const char *	inet_ntop(int, const void *, char *, size_t);
extern u_int		inet_nsap_addr(const char *, u_char *, int);
extern char *		inet_nsap_ntoa(int, const u_char *, char *);
extern int		inet_isaddr(const char *, uint32_t *);
#endif  /* _SGIAPI */

#ifdef __cplusplus
}
#endif

#endif /* !__ARPA_INET_H__ */
