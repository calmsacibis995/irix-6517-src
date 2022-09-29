/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)mkstemp.c	5.2 (Berkeley) 3/9/86";
#endif
#ifdef __STDC__
	#pragma weak mkstemp = _mkstemp
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include "shlib.h"
#include "mplib.h"

#ifdef sgi
#include <fcntl.h>
#else
#include <sys/file.h>
#endif

int
mkstemp(char *as)
{
	register char *s;
	register pid_t pid;
	register int fd, i;
	LOCKDECLINIT(l, LOCKOPEN);

	pid = getpid();
	s = as;
	while (*s++)
		/* void */;
	s--;
	while (*--s == 'X') {
		*s = (char)((pid % 10) + '0');
		pid /= 10;
	}
	s++;
	i = 'a';
	while ((fd = open(as, O_CREAT|O_EXCL|O_RDWR, 0600)) == -1) {
		if (i == 'z') {
			UNLOCKOPEN(l);
			return(-1);
		}
		*s = (char)i++;
	}
	UNLOCKOPEN(l);
	return(fd);
}
