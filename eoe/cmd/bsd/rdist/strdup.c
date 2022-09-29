#ifndef lint
static char *RCSid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/rdist/RCS/strdup.c,v 1.1 1994/06/02 23:21:19 jes Exp $";
#endif

/*
 * $Log: strdup.c,v $
 * Revision 1.1  1994/06/02 23:21:19  jes
 * Initial revision
 *
 * Revision 1.2  1992/04/16  01:28:02  mcooper
 * Some de-linting.
 *
 * Revision 1.1  1992/03/21  02:48:11  mcooper
 * Initial revision
 *
 */


#include <stdio.h>

/*
 * Most systems don't have this (yet)
 */
char *strdup(str)
     char *str;
{
    char 		       *p;
    extern char		       *malloc();
    extern char		       *strcpy();

    if ((p = malloc(strlen(str)+1)) == NULL)
	return((char *) NULL);

    (void) strcpy(p, str);

    return(p);
}
