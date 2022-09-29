/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 1.6 $"
#ident	"@(#)pwd:pwd.c	1.14.1.2"
/*
**	Print working (current) directory
*/

#include	<stdio.h>
#include	<unistd.h>
#include	<limits.h>
#include	<locale.h>
#include	<pfmt.h>

char	name[PATH_MAX+1];

main()
{
	int length;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:pwd");

	if (getcwd(name, PATH_MAX + 1) == (char *)0) {
		pfmt(stderr, MM_ERROR, ":129:Cannot determine current directory");
		putc('\n', stderr);
		exit(2);
	}
	length = strlen(name);
	name[length] = '\n';
	write(1, name, length + 1);
	exit(0);
}
