/*
 * Copyright (c) 1987 Regents of the University of California.
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
 */

#ifdef __STDC__
	#pragma weak h_errlist = _h_errlist
	#pragma weak h_nerr    = _h_nerr
	#pragma weak herror    = _herror
	#pragma weak hstrerror = _hstrerror
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)herror.c	6.5 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/uio.h>
#include <netdb.h>
#include <string.h>	/* prototype for strlen() */
#include <unistd.h>	/* prototype for readv/writev() */

char	*h_errlist[] = {
	"Error 0",
	"Unknown host",				/* 1 HOST_NOT_FOUND */
	"Host name lookup failure",		/* 2 TRY_AGAIN */
	"Unknown server error",			/* 3 NO_RECOVERY */
	"No address associated with name",	/* 4 NO_ADDRESS */
};
size_t	h_nerr = { sizeof(h_errlist)/sizeof(h_errlist[0]) };

/*
 * herror --
 *	print the error indicated by the h_errno value.
 */
void
herror(const char *s)
{
	struct iovec iov[4];
	register struct iovec *v = iov;

	if (s && *s) {
		v->iov_base = (caddr_t)s;
		v->iov_len = (int)strlen(s);
		v++;
		v->iov_base = ": ";
		v->iov_len = 2;
		v++;
	}
	v->iov_base = (size_t)h_errno < h_nerr ?
	    h_errlist[h_errno] : "Unknown error";
	v->iov_len = (int)strlen(v->iov_base);
	v++;
	v->iov_base = "\n";
	v->iov_len = 1;
	writev(2, iov, (int)(v - iov) + 1);
}

char *
hstrerror(int err)
{
	return ((size_t)err < h_nerr ? h_errlist[err] : "Unknown resolver error");
}
