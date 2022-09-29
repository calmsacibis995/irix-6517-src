/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

#include "con.h"

extern void	spew(int);

void
erase(void)
{
	int i;

	for(i=0; i<11*(VERTRESP/VERTRES); i++)
		spew(DOWN);
	return;
}
