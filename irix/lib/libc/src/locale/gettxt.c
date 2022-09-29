/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/gettxt.c	1.9"

/*	gettxt()
 *	Uses __gtxt() to return the message (or the default)
 */
#ifdef __STDC__
	#pragma weak gettxt = _gettxt
#endif
#include "synonyms.h"
#include <ctype.h>
#include <pfmt.h>

char *
gettxt(msgid, dflt)
const char *msgid, *dflt;
{
	char catname[DB_NAME_LEN];
	register int msgnum = 0;
	register int i, oldi;
	char c;
	
	/* Extract the name of the catalogue */
	for (i = 0 ; i < DB_NAME_LEN && (c = msgid[i]) && c != ':' ; i++)
		catname[i] = c;
	/* Extract the message number */
	if (i != DB_NAME_LEN && c){
		oldi = i;
		catname[i] = '\0';
		while (isdigit(c = msgid[++i])){
			msgnum *= 10;
			msgnum += c - '0';
		}
		if (c || oldi == i - 1)
			msgnum = -1;
	}
	else
		msgnum = -1;

	return((char *)__gtxt(catname, msgnum, dflt));
}
