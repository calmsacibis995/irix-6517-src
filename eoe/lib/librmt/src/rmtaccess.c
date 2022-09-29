#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtaccess.c,v 1.1 1988/12/07 16:33:19 lindy Exp $"

/*
 *	Test pathname for specified access.  Looks just like access(2)
 *	to caller.
 */
 
int rmtaccess (path, amode)
char *path;
int amode;
{
	if (_rmt_dev (path))
	{
		return (0);		/* Let /etc/rmt find out */
	}
	else
	{
		return (access (path, amode));
	}
}


