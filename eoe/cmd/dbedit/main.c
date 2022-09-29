/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * #ident "$Revision: 1.4 $"
 *
 * dbedit - edit a system database
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/mac.h>
#include "dbedit.h"

char	lock_path[MAXPATHLEN];
int	lock_fd;
int	mac_enabled;

int
dbe_lock(char *path)
{

	sprintf(lock_path, "%s.lock", path);

	if ((lock_fd = open(lock_path, O_CREAT | O_EXCL, 0600)) < 0)
		return errno;
	return 0;
}

void
dbe_unlock()
{
	if (lock_fd >= 0) {
		(void) unlink(lock_path);
		(void) close(lock_fd);
	}
	return;
}

main(int argc, char *argv[])
{
	int i;
	int why;
	int error = 0;
	char *program = argv[0];	/* Program name saved */
	char *path = NULL;		/* File pathname */
	char *name;
	line_t *head;			/* The line list */
	mac_t plabel;			/* label of this process */
	mac_t flabel;			/* label of the file */


	mac_enabled = sysconf(_SC_MAC) > 0;

	if (mac_enabled && (plabel = mac_get_proc()) == NULL) {
		fprintf(stderr, "%s: process label failure.", program);
		exit(1);
	}

	setbuf(stdin, NULL);
	setbuf(stdout, NULL);

	for (i = 1; i < argc; i++) {
		/*
		 * Find a file name to go with the database name passed.
		 * Check both the database names and the database paths.
		 * If none's found, assume this is a path name.
		 */
		path = find_database(argv[i]);

		/*
		 * Verify write access
		 */
		if (access(path, W_OK) != 0) {
			fprintf(stderr, "%s: Write access to \"%s\" denied.\n",
			    program, argv[i]);
			continue;
		}

		/*
		 * Lock the file.
		 */
		if (why = dbe_lock(path)) {
			if (why == EEXIST)
				fprintf(stderr,
				    "%s: Database \"%s\" is locked.\n",
				    program, argv[i]);
			else
				fprintf(stderr,
				    "%s: locking Database \"%s\" failed.\n",
				    program, argv[i]);
			
			switch (confirm("Attempt to continue? [nyq]: ",
			    "NnYyQq")) {
			case 'n':
			case 'N':
				continue;
			case 'y':
			case 'Y':
				break;
			case 'q':
			case 'Q':
				exit(1);
			}
		}

		if (mac_enabled && (flabel = mac_get_file(path)) == NULL) {
			fprintf(stderr,
			   "%s: MAC label of \"%s\" is unobtainable.\n",
			    program, argv[i]);
			dbe_unlock();
			continue;
		}
		/*
		 * Process the database.
		 */
		if ((head = read_db(path)) == NULL) {
			fprintf(stderr, "%s: Cannot load \"%s\".\n",
			    program, argv[i]);
			dbe_unlock();
			continue;
		}
		head = process_db(head, argv[i]);
		audit_changes(head, argv[i]);
		save_db(head, path);

		if (mac_enabled && !mac_equal(plabel, flabel))
			mac_set_file(path, flabel);

		/*
		 * Unlock the file.
		 */
		dbe_unlock();
	}
	exit(error);
}
