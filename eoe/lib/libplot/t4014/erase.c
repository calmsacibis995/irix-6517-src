/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

#include <unistd.h>

extern int	putch(int);

extern int ohiy;
extern int ohix;
extern int oloy;
extern int oextra;

void
erase(void)
{
	int i;

	putch(033);
	putch(014);
	ohiy= -1;
	ohix = -1;
	oextra = -1;
	oloy = -1;
	sleep(2);
	return;
}
