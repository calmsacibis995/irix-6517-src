/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/raise.c	1.4"
/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak raise = _raise
#endif
#include <sys/types.h>
#include "synonyms.h"
#include <signal.h>
#include <unistd.h>
#include "mplib.h"


int
raise(sig)
int sig;
{
	MTLIB_RETURN( (MTCTL_RAISE, sig) );
	return( kill(getpid(), sig) );
}
