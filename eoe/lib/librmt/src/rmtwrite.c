#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtwrite.c,v 1.3 1995/03/15 17:56:34 tap Exp $"

#include <signal.h>
#include <errno.h>

#include "rmtlib.h"

static int _rmt_write(int, char *, unsigned int);

/*
 *	Write to stream.  Looks just like write(2) to caller.
 */
 
int rmtwrite (fildes, buf, nbyte)
int fildes;
char *buf;
unsigned int nbyte;
{
	if (isrmt (fildes))
	{
		return (_rmt_write (fildes - REM_BIAS, buf, nbyte));
	}
	else
	{
		return (write (fildes, buf, nbyte));
	}
}


/*
 *	_rmt_write --- write a buffer to the remote tape
 */

static int _rmt_write(int fildes, char *buf, unsigned int nbyte)
{
	char buffer[BUFMAGIC];
	void (*pstat)();

	sprintf(buffer, "W%d\n", nbyte);
	if (_rmt_command(fildes, buffer) == -1)
		return(-1);

	pstat = signal(SIGPIPE, SIG_IGN);
	if (write(WRITE(fildes), buf, nbyte) == nbyte)
	{
		signal (SIGPIPE, pstat);
		return(_rmt_status(fildes));
	}

	signal (SIGPIPE, pstat);
	_rmt_abort(fildes);
	setoserror( EIO );
	return(-1);
}
