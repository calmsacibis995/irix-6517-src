#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtdev.c,v 1.1 1988/12/07 16:33:21 lindy Exp $"

#ifdef BSD
#define strchr index
#endif

/*
 *	Test pathname to see if it is local or remote.  A remote device
 *	is any string that contains ":/dev/".  Returns 1 if remote,
 *	0 otherwise.
 */
 
int _rmt_dev (path)
register char *path;
{
        extern char *strchr ();

	if ((path = strchr (path, ':')) != (char *)0)
	{
		if (strncmp (path + 1, "/dev/", 5) == 0)
		{
			return (1);
		}
	}
	return (0);
}


