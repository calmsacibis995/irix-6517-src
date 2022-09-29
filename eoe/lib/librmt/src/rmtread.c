#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtread.c,v 1.3 1995/03/15 17:56:11 tap Exp $"

#include <errno.h>

#include "rmtlib.h"

static int _rmt_read(int, char *, unsigned int);

/*
 *	Read from stream.  Looks just like read(2) to caller.
 */
  
int rmtread (fildes, buf, nbyte)
int fildes;
char *buf;
unsigned int nbyte;
{
	if (isrmt (fildes))
	{
		return (_rmt_read (fildes - REM_BIAS, buf, nbyte));
	}
	else
	{
		return (read (fildes, buf, nbyte));
	}
}


/*
 *	_rmt_read --- read a buffer from a remote tape
 */

static int _rmt_read(int fildes, char *buf, unsigned int nbyte)
{
	int rc, i;
	char buffer[BUFMAGIC];

	sprintf(buffer, "R%d\n", nbyte);
	if (_rmt_command(fildes, buffer) == -1 || (rc = _rmt_status(fildes)) == -1)
		return(-1);

	for (i = 0; i < rc; i += nbyte, buf += nbyte)
	{
		nbyte = read(READ(fildes), buf, rc);
		if (nbyte <= 0)
		{
			_rmt_abort(fildes);
			setoserror( EIO );
			return(-1);
		}
	}

	return(rc);
}
