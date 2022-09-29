#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/isrmt.c,v 1.1 1988/12/07 16:33:17 lindy Exp $"

#include "rmtlib.h"

/*
 *	Isrmt. Let a programmer know he has a remote device.
 */

int isrmt (fd)
int fd;
{
	return (fd >= REM_BIAS);
}
