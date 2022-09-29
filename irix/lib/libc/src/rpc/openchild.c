/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.7 88/02/08 
 */

/*
 * Open two pipes to a child process, one for reading, one for writing.
 * The pipes are accessed by FILE pointers. This is NOT a public
 * interface, but for internal use only!
 */
#include "synonyms.h"
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>	/* prototype for pipe() */
#include <stdlib.h> 	/* prototype for malloc() */
#include <paths.h> 	/* for shell path */
#include <netinet/in.h>
#include "rpc_extern.h"

static char *basename(char *);

/*
 * returns pid, or -1 for failure
 */
int
_openchild(char *command, FILE **fto, FILE **ffrom)
{
	int i;
	int pid;
	int pdto[2];
	int pdfrom[2];
	char *com;

		
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
		(void) dup(pdto[0]);
		(void) close(1); 
		(void) dup(pdfrom[1]);
		for (i = _rpc_dtablesize() - 1; i >= 3; i--) {
			(void) close(i);
		}
		com = malloc((unsigned) strlen(command) + 6);
		if (com == NULL) {
			_exit(~0);
		}	
		(void) sprintf(com, "exec %s", command);
		execl(_PATH_BSHELL, basename(_PATH_BSHELL), "-c", com, NULL);
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

static char *
basename(char *path)
{
	char *p;

	p = strrchr(path, '/');
	if (p == NULL) {
		return (path);
	} else {
		return (p + 1);
	}
}
