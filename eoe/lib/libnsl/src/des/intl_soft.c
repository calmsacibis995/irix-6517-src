/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)libnsl:des/intldes_soft.c	1.1.4.2"
#ifdef __STDC__
#ifndef _LIBNSL_ABI
        #pragma weak des_setparity	= _des_setparity
#endif /* _LIBNSL_ABI */
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)des_soft.c 1.2 89/03/10 Copyr 1986 Sun Micro";
#endif

/* #include <sys/types.h> */
/* #include <des/softdes.h> */
#ifndef sgi
#include <des/des.h> 
#endif

/* ARGSUSED */
void
des_setparity(p)
	char *p;
{
}

/* ARGSUSED */
__des_crypt(buf, len, desp)
	register char *buf;
	register unsigned len;
#ifdef sgi
	void *desp;
#else
	struct desparams *desp;
#endif
{
	return (1);
}
