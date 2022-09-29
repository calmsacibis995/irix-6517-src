/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)librpc:openchild.c	1.1.4.1"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 
/*
 * openchild.c,
 *
 * Open two pipes to a child process, one for reading, one for writing. The
 * pipes are accessed by FILE pointers. This is NOT a public interface, but
 * for internal use only!
 */
#if defined(__STDC__) 
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

#include <stdio.h>

/*
 * returns pid, or -1 for failure
 */
_rpc_openchild(command, fto, ffrom)
	char	*command;
	FILE	**fto;
	FILE	**ffrom;
{
	int	i;
	int	pid;
	int	pdto[2];
	int	pdfrom[2];

	if (pipe(pdto) < 0) {
		goto error1;
	}
	if (pipe(pdfrom) < 0) {
		goto error2;
	}
	switch (pid = fork()) {
	case -1:
		goto error3;

	case 0:
		/*
		 * child: read from pdto[0], write into pdfrom[1]
		 */
		(void) close(0);
		dup(pdto[0]);
		(void) close(1);
		dup(pdfrom[1]);
		
		fflush(stderr);
		for (i = _rpc_dtbsize() - 1; i >= 3; i--) {
			(void) close(i);
		}
		fflush(stderr);
		execlp(command, command, 0);
		perror("exec");
		_exit(~0);

	default:
		/*
		 * parent: write into pdto[1], read from pdfrom[0]
		 */
		*fto = fdopen(pdto[1], "w");
		(void) close(pdto[0]);
		*ffrom = fdopen(pdfrom[0], "r");
		(void) close(pdfrom[1]);
		break;
	}
	return (pid);

	/*
	 * error cleanup and return
	 */
error3:
	(void) close(pdfrom[0]);
	(void) close(pdfrom[1]);
error2:
	(void) close(pdto[0]);
	(void) close(pdto[1]);
error1:
	return (-1);
}
