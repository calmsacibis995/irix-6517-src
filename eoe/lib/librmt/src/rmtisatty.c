#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtisatty.c,v 1.1 1988/12/07 16:33:23 lindy Exp $"

/*
 *	Rmtisatty.  Do the isatty function.
 */

int rmtisatty (fd)
int fd;
{
	if (isrmt (fd))
	{
		return (0);
	}
	else
	{
		return (isatty (fd));
	}
}
