/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wmisc/wisprint.c	1.1.2.2"

#include <widec.h>
#include <stdlib.h>
#include <ctype.h>
#include "_wchar.h"

int
wisprint(wchar_t c)
{
	if((int)c > 0377)
		return(1);
	if((int)c < 0 || c < 0240 && !isprint(c))
		return(0);
	return(1);
}
