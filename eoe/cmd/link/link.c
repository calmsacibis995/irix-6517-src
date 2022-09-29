/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)link:link.c	1.2" */
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/link/RCS/link.c,v 1.3 1992/04/27 12:33:23 wtk Exp $"

/***************************************************************************
 * Command: link
 * Inheritable Privileges: P_MACREAD,P_DACREAD,P_MACWRITE,P_DACWRITE,P_FILESYS
 *       Fixed Privileges: None
 * Notes: This command calls the link(2) system call directly.
 *	  The privileges passed to command are only used by link(2).
 *
 ***************************************************************************/

main(argc, argv)
int argc;
char *argv[];
{
	if(argc!=3) {
		write(2, "Usage: /usr/sbin/link from to\n", 30);
		exit(1);
	}
	exit((link(argv[1], argv[2])==0)? 0: 2);
}
