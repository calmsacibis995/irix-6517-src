#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtstatus.c,v 1.2 1995/03/15 17:56:21 tap Exp $"

#include <errno.h>

#include "rmtlib.h"

/*
 *	_rmt_status --- retrieve the status from the pipe
 */

int _rmt_status(fildes)
int fildes;
{
	int i;
	char c, *cp;
	char buffer[BUFMAGIC];

/*
 *	read the reply command line
 */

	for (i = 0, cp = buffer; i < BUFMAGIC; i++, cp++)
	{
		if (read(READ(fildes), cp, 1) != 1)
		{
			_rmt_abort(fildes);
			setoserror( EIO );
			return(-1);
		}
		if (*cp == '\n')
		{
			*cp = 0;
			break;
		}
	}

	if (i == BUFMAGIC)
	{
		_rmt_abort(fildes);
		setoserror( EIO );
		return(-1);
	}

/*
 *	check the return status
 */

	for (cp = buffer; *cp; cp++)
		if (*cp != ' ')
			break;

	if (*cp == 'E' || *cp == 'F')
	{
		setoserror( atoi(cp + 1) );
		while (read(READ(fildes), &c, 1) == 1)
			if (c == '\n')
				break;

		if (*cp == 'F')
			_rmt_abort(fildes);

		return(-1);
	}

/*
 *	check for mis-synced pipes
 */

	if (*cp != 'A')
	{
		_rmt_abort(fildes);
		setoserror( EIO );
		return(-1);
	}

	return(atoi(cp + 1));
}



