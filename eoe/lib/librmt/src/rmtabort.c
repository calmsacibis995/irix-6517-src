#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtabort.c,v 1.2 1992/08/02 20:54:07 ism Exp $"

#include "rmtlib.h"

/*
 *	abort --- close off a remote tape connection
 */

void _rmt_abort(int fildes)
{
	close(READ(fildes));
	close(WRITE(fildes));
	READ(fildes) = -1;
	WRITE(fildes) = -1;
}
