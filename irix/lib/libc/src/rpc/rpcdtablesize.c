#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = 	"@(#)rpcdtablesize.c	1.2 88/07/27 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.3 88/02/08 
 * Used to be in file: rpc_dtablesize.c
 */

#include "synonyms.h"
#include <unistd.h>

/*
 * Cache the result of getdtablesize(), so we don't have to do an
 * expensive system call every time.
 */

int
_rpc_dtablesize(void)
{
	static int size;
	
	if (size == 0) {
		size = getdtablesize();
	}
	return (size);
}
