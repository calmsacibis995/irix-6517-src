#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtlseek.c,v 1.5 1995/08/22 03:58:19 doucette Exp $"

#include <sys/types.h>
#include "rmtlib.h"

static off_t _rmt_lseek(int, off_t, int);

/*
 *	Perform lseek on file.  Looks just like lseek(2) to caller.
 */

off_t rmtlseek (fildes, offset, whence)
int fildes;
off_t offset;
int whence;
{
	if (isrmt (fildes))
	{
		return (_rmt_lseek (fildes - REM_BIAS, offset, whence));
	}
	else
	{
		return (lseek (fildes, offset, whence));
	}
}


/*
 *	_rmt_lseek --- perform an imitation lseek operation remotely
 */

static off_t _rmt_lseek(int fildes, off_t offset, int whence)
{
	char buffer[BUFMAGIC];

	sprintf(buffer, "L%d\n%d\n", offset, whence);
	if (_rmt_command(fildes, buffer) == -1)
		return(-1);

	return(_rmt_status(fildes));
}


