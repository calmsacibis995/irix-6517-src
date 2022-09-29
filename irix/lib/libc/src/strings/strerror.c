/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/strerror.c	1.6"
/*LINTLIBRARY*/
#include "synonyms.h"
#include <string.h>
#include <stddef.h>
#include <pfmt.h>
#include "sys/errno.h"
#include "priv_extern.h"

char *
strerror(int errnum)
{
	if (errnum >= __IRIXBASE)
		return (__irixerror(errnum));
	else if (errnum >= _sys_nerr)
		return (__svr4error(errnum));
	else if (errnum >= 0)
		return (__sys_errlisterror(errnum));
	else
		return(NULL);
}
