#ident	"lib/libsc/lib/path.c:  $Revision: 1.2 $"

/*
 * path.c - create default path from current environ variables
 */

#include <libsc.h>

/*
 * make_path - create a reasonable default device search path 
 *	from environment variables
 */
char *
makepath(void)
{
	char *cp, *pb;
	static char pathbuf[128];

	pb = pathbuf;
	*pb = '\0';
	for (cp = getenv("SystemPartition"); cp && *cp; cp++ )
	    if (*cp == ';') {
		*pb++ = '/';
		*pb++ = ' ';
	    } else 
		*pb++ = *cp;

	if (cp) {
	    *pb++ = '/';
	    *pb++ = ' ';
	}

	for (cp = getenv("OSLoadPartition"); cp && *cp; cp++ )
	    if (*cp == ';') {
		*pb++ = '/';
		*pb++ = ' ';
	    } else 
		*pb++ = *cp;

	if (cp) {
	    *pb++ = '/';
	    *pb++ = ' ';
	}

	if (pathbuf[0] == '\0')
	    return 0;

	return &pathbuf[0];
}
