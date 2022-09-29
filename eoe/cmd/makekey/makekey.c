/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)makekey:makekey.c	1.2"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/makekey/RCS/makekey.c,v 1.4 1992/04/27 12:07:04 wtk Exp $"
/*
 * You send it 10 bytes.
 * It sends you 13 bytes.
 * The transformation is expensive to perform
 * (a significant part of a second).
 */

char	*crypt();

main()
{
	char key[8];
	char salt[2];
	
	read(0, key, 8);
	read(0, salt, 2);
	write(1, crypt(key, salt), 13);
	return(0);
}
