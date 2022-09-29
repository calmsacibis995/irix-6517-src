/* domain.h - domain header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *      @(#)domain.h    7.3 (Berkeley) 6/27/88
 */

/*
modification history
--------------------
02g,22sep92,rrr  added support for c++
02f,26may92,rrr  the tree shuffle
02e,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
02d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02c,16apr89,gae  updated to new 4.3BSD.
02b,04nov87,dnw  moved declaration of domains structure to uipc_dom.c.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCdomainh
#define __INCdomainh

#ifdef __cplusplus
extern "C" {
#endif

/* Structure per communications domain. */

struct	domain
    {
    int		dom_family;		/* AF_xxx */
    char	*dom_name;
    int		(*dom_init)();		/* initialize domain data structures */
    int		(*dom_externalize)();	/* externalize access rights */
    int		(*dom_dispose)();	/* dispose of internalized rights */
    struct	protosw *dom_protosw;
    struct	protosw *dom_protoswNPROTOSW;
    struct	domain *dom_next;
    };

#ifdef __cplusplus
}
#endif

#endif /* __INCdomainh */
