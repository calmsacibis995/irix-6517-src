#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtclose.c,v 1.2 1992/08/02 20:54:13 ism Exp $"

#include "rmtlib.h"

static int _rmt_close(int);
/*
 *	Close a file.  Looks just like close(2) to caller.
 */
 
int rmtclose (fildes)
int fildes;
{
	if (isrmt (fildes))
	{
		return (_rmt_close (fildes - REM_BIAS));
	}
	else
	{
		return (close (fildes));
	}
}

/*
 *	_rmt_close --- close a remote magtape unit and shut down
 */

static int _rmt_close(int fildes)
{
	int rc;

	if (_rmt_command(fildes, "C\n") != -1)
	{
		rc = _rmt_status(fildes);

		_rmt_abort(fildes);
		return(rc);
	}

	return(-1);
}


