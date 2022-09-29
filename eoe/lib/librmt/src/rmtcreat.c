#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtcreat.c,v 1.1 1988/12/07 16:33:20 lindy Exp $"

/*
 *	Create a file from scratch.  Looks just like creat(2) to the caller.
 */

#ifdef BSD
#include <sys/file.h>		/* BSD DEPENDANT!!! */
#else
#include <fcntl.h>		/* use this one for S5 with remote stuff */
#endif

int rmtcreat (path, mode)
char *path;
int mode;
{
	if (_rmt_dev (path))
	{
		return (rmtopen (path, 1 | O_CREAT, mode));
	}
	else
	{
		return (creat (path, mode));
	}
}


