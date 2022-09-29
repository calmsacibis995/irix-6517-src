#ifndef lint
static char *RCSid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/rdist/RCS/strerror.c,v 1.1 1994/06/02 23:21:21 jes Exp $";
#endif

/*
 * $Log: strerror.c,v $
 * Revision 1.1  1994/06/02 23:21:21  jes
 * Initial revision
 *
 * Revision 1.1  1992/03/21  02:48:11  mcooper
 * Initial revision
 *
 */

#include <stdio.h>
#include <sys/errno.h>

/*
 * Return string for system error number "Num".
 */
char *strerror(Num)
     int			Num;
{
    extern int 			sys_nerr;
    extern char 	       *sys_errlist[];
    static char 		Unknown[100];

    if (Num < 0 || Num > sys_nerr) {
	(void) sprintf(Unknown, "Error %d", Num);
	return(Unknown);
    } else
	return(sys_errlist[Num]);
}
