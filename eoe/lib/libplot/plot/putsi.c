/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

#include <stdio.h>

void
putsi(int a)
{
	putc((char)a,stdout);
	putc((char)(a>>8),stdout);
}
