/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)logname:logname.c	1.4"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/logname/RCS/logname.c,v 1.4 1990/03/01 18:28:14 bowen Exp $"

#include <stdio.h>
main() {
	char *name;

	name = cuserid((char *)NULL);
	if (name == NULL)
		return (1);
	(void) puts (name);
	return (0);
}
