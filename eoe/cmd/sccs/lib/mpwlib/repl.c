/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sccs:lib/mpwlib/repl.c	6.3"
/*
	Replace each occurrence of `old' with `new' in `str'.
	Return `str'.
*/

char *
repl(str,old,new)
char *str;
char old,new;
{
	char	*trnslat();
	static char os[2], ns[2]; /* ensure null terminated strings! */
	os[0] = old; ns[0] = new;
	return(trnslat(str, os, ns, str));
}
