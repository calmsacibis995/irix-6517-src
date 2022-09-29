/* Become a daemon.
 *
 * Copyright 1991, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.9 $"
#ifdef __STDC__
	#pragma weak _daemonize = __daemonize
#endif
#include "synonyms.h"
#include <sys/file.h>
#include <sys/types.h>		/* for prototyping */
#include <sys/stat.h>		/* for prototyping */
#include <fcntl.h>		/* for prototyping */
#include <unistd.h>
#include <syslog.h>
#include <paths.h>
#include "priv_extern.h"

/*
 * Become a daemon.
 *	Normally fork, change directory to /, and close all files.
 *	Optionally keep some files open.
 */
int					/* -1=failed */
_daemonize(int flags,			/* whether to fork, chdir, and close */
	   int fd1, int fd2, int fd3)	/* keep these open */
{
	int i;

	if ((flags & _DF_NOFORK) == 0) {
		i = fork();
		if (i < 0)
			return -1;
		if (i != 0)
			exit(0);
		(void)setsid();
	}

	if ((flags & _DF_NOCHDIR) == 0)
		(void)chdir("/");

	if ((flags & (_DF_NOCLOSE|_DF_NOFORK)) != (_DF_NOCLOSE|_DF_NOFORK)) {
		_yp_unbind_all();	/* tell NIS its FD is dead */
		_res_close();		/* tell resolver its FD is dead */
	}

	if ((flags & _DF_NOCLOSE) == 0) {
		for (i = getdtablehi(); --i >= 0; ) {
			if (i != fd1 && i != fd2 && i != fd3)
				(void)close(i);
		}

		closelog();		/* since we just did it implicitly */

		i = open(_PATH_DEVNULL, O_RDWR, 0);
		if (i >= 0) {
			if (fd1 != 0 && fd2 != 0 && fd3 != 0)
				(void)dup2(i, 0);
			if (fd1 != 1 && fd2 != 1 && fd3 != 1)
				(void)dup2(i, 1);
			if (fd1 != 2 && fd2 != 2 && fd3 != 2)
				(void)dup2(i, 2);
			if (i > 2)
				(void)close(i);
		}
	}

	return 0;
}
