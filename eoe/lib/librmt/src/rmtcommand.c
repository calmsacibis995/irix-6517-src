#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtcommand.c,v 1.3 1995/03/15 17:55:18 tap Exp $"

#include <signal.h>
#include <errno.h>

#include "rmtlib.h"


/*
 *	_rmt_command --- attempt to perform a remote tape command
 */

int _rmt_command(fildes, buf)
int fildes;
char *buf;
{
	register int blen;
	void (*pstat)();

/*
 *	save current pipe status and try to make the request
 */

	blen = strlen(buf);
	pstat = signal(SIGPIPE, SIG_IGN);
	if (write(WRITE(fildes), buf, blen) == blen)
	{
		signal(SIGPIPE, pstat);
		return(0);
	}

/*
 *	something went wrong. close down and go home
 */

	signal(SIGPIPE, pstat);
	_rmt_abort(fildes);

	setoserror( EIO );
	return(-1);
}



