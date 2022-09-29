/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

#ident	"@(#)libnsl:common/lib/libnsl/nsl/t_strerror.c	1.3"

#ifdef __STDC__
        #pragma weak t_strerror	= _t_strerror
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
#include <xti.h>
#include "_import.h"

extern  char *t_errlist(int);

char *
t_strerror(errnum)
int errnum;
{
	return(t_errlist(errnum));
}
