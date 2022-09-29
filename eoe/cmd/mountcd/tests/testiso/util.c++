/*
 *  util.c++
 *
 *  Description:
 *	Utility routines for testiso
 *
 *  History:
 *      rogerc      04/12/91    Created
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <bstring.h>
#include <stdio.h>
#include "util.h"

void
error(int err, char *str)
{
    if (err) {
	errno = err;
	perror(str);
    }
    exit(-1);
}

/*
 *  char *
 *  to_unixname(char *name, int length, int notranslate)
 *
 *  Description:
 *      Convert an ISO 9660 name to a UNIX name.  We convert to all lower case
 *      letters, and blow away any fluff of the form ";#".
 *
 *  Parameters:
 *      name
 *      length
 *	notranslate
 *
 *  Returns:
 *      The unix name.  Note: this is static, will be overwritten next
 *      this function is called
 */

char *
to_unixname(char *name, int length, int notranslate)
{
#define min(a,b) ((a) < (b) ? (a) : (b))
    static char uname[300];
    static char *dot = ".";
    static char *dotdot = "..";
    int         i;
    char        *dotp;

    length = min(sizeof (uname) - 1, length);

    if (*name == '\0')
	return (dot);
    if (*name == '\1')
	return (dotdot);

    if (notranslate) {
	bcopy(name, uname, length);
	uname[length] = '\0';
	return (uname);
    }

    for (i = 0; i < length; i++) {
	if (name[i] == ';') {
	    uname[i] = '\0';
	    break;
	}
	uname[i] = tolower(name[i]);
    }
    uname[i] = '\0';

    dotp = strrchr(uname, '.');

    if (dotp && *(dotp + 1) == '\0')
	*dotp = '\0';

    return (uname);
#undef min
}
